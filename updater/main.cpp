// @sylefeb 2024-09-05

/*

TODO:
- hide attribute

*/

#include <vector>
#include <string>
#include <cstdint>
#include "sexpresso.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

#include <map>
#include <LibSL/CppHelpers/CppHelpers.h>
#include <LibSL/Errors/Errors.h>

#include <tclap/CmdLine.h>

class PCBDesign
{
private:

  std::map<std::string,sexpresso::Sexp*> m_FootprintsByRef;

  sexpresso::Sexp m_Root;

  std::string getFootprintRef(sexpresso::Sexp& se)
  {
    if (se.childCount() > 2) {
      if ( se.getChild(0).isString()
        && se.getChild(1).isString()
        && se.getChild(2).isString()) {
          if (   se.getChild(0).value.str == "fp_text"
              && se.getChild(1).value.str == "reference") {
                return se.getChild(2).value.str;
          }
      }
    }
    for (int c = 0 ; c < se.childCount() ; ++c) {
      if (se.getChild(c).isSexp()) {
        std::string r = getFootprintRef(se.getChild(c));
        if (!r.empty()) {
          return r;
        }
      }
    }
    return "";
  }

  void extractFootprints(sexpresso::Sexp& se)
  {
    if (se.childCount() > 0) {
      bool skip = false;
      if (se.getChild(0).isString()) {
        if (se.getChild(0).value.str == "footprint") {
          std::string ref = getFootprintRef(se);
          m_FootprintsByRef[ref] = &se;
          skip = true; // no need to go down
        }
      }
      if (!skip) {
        for (int c = 0 ; c < se.childCount() ; ++c) {
          if (se.getChild(c).isSexp()) {
            extractFootprints(se.getChild(c));
          }
        }
      }
    }
  }

public:

  PCBDesign(std::string fname)
  {
    std::ifstream     t(fname);
    std::stringstream buffer;
    if (!t) {
      throw LibSL::Errors::Fatal("Cannot read from %s",fname.c_str());
    }
    buffer << t.rdbuf();
    m_Root = sexpresso::parse(buffer.str());
    extractFootprints(m_Root);
  }

  void importFootprints(PCBDesign& source)
  {
    // get the kicad_pcb roots
    sl_assert(m_Root.childCount() > 0);
    sexpresso::Sexp& d_kicad = m_Root.getChild(0);
    sl_assert(source.root().childCount() > 0);
    sexpresso::Sexp& s_kicad = source.root().getChild(0);
    for (auto [sfp,sse] : source.footprintsByRef()) {
      // find corresponding footprint in this design
      auto D = m_FootprintsByRef.find(sfp);
      if (D != m_FootprintsByRef.end()) { // match!
        // replace the entire footprint
        /// NOTE: initially wanted to be more subtle, but many internal nodes
        ///       are not easily specified ; this might require have command
        ///       line capabilities to not touch certain aspects of a new incoming
        ///       footprint, as this will always erase by the old one
        transferFootprint(*D->second,*sse);
      }
    }
  }

  bool isPad(const sexpresso::Sexp& n)
  {
    if (n.isString()) return false;
    if (n.childCount() > 1) {
      if (n.getChild(0).isString()) {
        if (n.getChild(0).value.str == "pad" && n.getChild(1).isString()) {
          // pad!
          return true;
        }
      }
    }
    return false;
  }

  bool isNet(const sexpresso::Sexp& n)
  {
    if (n.isString()) return false;
    if (n.childCount() > 1) {
      if (n.getChild(0).isString()) {
        if (n.getChild(0).value.str == "net" && n.getChild(1).isString()) {
          // net!
          return true;
        }
      }
    }
    return false;
  }

  const sexpresso::Sexp& getPad(std::string name,const sexpresso::Sexp& footprint)
  {
    for (int c = 0 ; c < footprint.childCount() ; ++c) {
      if (isPad(footprint.getChild(c))) {
        if (footprint.getChild(c).getChild(1).value.str == name) {
          return footprint.getChild(c);
        }
      }
    }
    throw LibSL::Errors::Fatal("could not find pad %s",name.c_str());
    static sexpresso::Sexp not_found;
    return not_found;
  }

  const sexpresso::Sexp& getNet(const sexpresso::Sexp& pad)
  {
    for (int c = 0 ; c < pad.childCount() ; ++c) {
      if (isNet(pad.getChild(c))) {
        return pad.getChild(c);
      }
    }
    throw LibSL::Errors::Fatal("could not find net in pad");
    static sexpresso::Sexp not_found;
    return not_found;
  }

  sexpresso::Sexp transferPad(const sexpresso::Sexp& src_pad, const sexpresso::Sexp& dst_footprint)
  {
    sexpresso::Sexp new_pad;
    std::cerr << "xfer pad " << src_pad.getChild(1).value.str << '\n';
    const sexpresso::Sexp& dst_pad = getPad(src_pad.getChild(1).value.str,dst_footprint);
    // from src: all but net
    for (int c = 0 ; c < src_pad.childCount() ; ++c) {
      if (!isNet(src_pad.getChild(c))) {
        new_pad.addChild(src_pad.getChild(c));
      }
    }
    // from dst: nets
    try {
      auto net = getNet(dst_pad);
      new_pad.addChild(net);
    } catch (...) {
      // no net in pad, ignore
    }
    return new_pad;
  }

  void transferFootprint(sexpresso::Sexp& dst, const sexpresso::Sexp& src)
  {
    // transfer data between matching src and dst footprints
    std::cerr << "footprint " << dst.getChild(1).value.str << '\n';
    // -> from src: all but pads
    sexpresso::Sexp new_node;
    for (int c = 0 ; c < src.childCount() ; ++c) {
      if (!isPad(src.getChild(c))) {
        // add child to new node
        new_node.addChild(src.getChild(c));
      } else {
        try {
          // stitch nets
          new_node.addChild(transferPad(src.getChild(c),dst));
        } catch (...) {
          std::cout << "pad does not exist in new design\n";
        }
      }
    }
    // overwrite dst
    dst = new_node;
  }

  void importNodes(PCBDesign& source)
  {
    // get the kicad_pcb roots
    sl_assert(m_Root.childCount() > 0);
    sexpresso::Sexp& d_kicad = m_Root.getChild(0);
    sl_assert(source.root().childCount() > 0);
    sexpresso::Sexp& s_kicad = source.root().getChild(0);
    // footprints
    for (auto [sfp, sse] : source.footprintsByRef()) {
      // find corresponding footprint in this design
      auto D = m_FootprintsByRef.find(sfp);
      if (D == m_FootprintsByRef.end()) { // not found
        // add back the footprint
        /// NOTE: well maybe this was trully removed?
        ///       add back only it is has no ref from skidl?
        d_kicad.addChild(*sse);
      }
    }
    // everything else
    for (int c = 0; c < s_kicad.childCount(); ++c) {
      if (s_kicad.getChild(c).isSexp()) {
        if (s_kicad.getChild(c).childCount() > 0) {
          if (s_kicad.getChild(c).getChild(0).isString()) {
            auto s = s_kicad.getChild(c).getChild(0).value.str;
            if (s != "version"
              && s != "generator"
              && s != "general"
              && s != "paper"
              && s != "layers"
              && s != "setup"
              && s != "net"
              && s != "footprint"
              ) {
              d_kicad.addChild(s_kicad.getChild(c));
            }
          }
        }
      }
    }
  }

  std::map<std::string,sexpresso::Sexp*> footprintsByRef() const {
    return m_FootprintsByRef;
  }

  sexpresso::Sexp& root() { return m_Root; }

  void save(std::string fname)
  {
    std::ofstream f(fname);
    f << m_Root.toString() << '\n';
    f.close();
  }

};

int main(int argc,const char **argv)
{
  try {

    TCLAP::CmdLine cmd(
      "<< skidl-updater >>\n"
      "(c) Sylvain Lefebvre -- @sylefeb\n"
      "Under GPLv3 license, see LICENSE_GPLv3 in repo root\n"
      ,' ', "v0.1");

    TCLAP::ValueArg<std::string> arg_prev("p", "prev", "Previous PCB file (.kicad_pcb)", true, "","string");
    cmd.add(arg_prev);
    TCLAP::ValueArg<std::string> arg_next("n", "next", "New PCB file (.kicad_pcb)", true, "","string");
    cmd.add(arg_next);
    TCLAP::ValueArg<std::string> arg_outp("o", "output", "Output PCB file (.kicad_pcb)", false, "out.kicad_pcb", "string");
    cmd.add(arg_outp);

    cmd.parse(argc,argv);

    std::cout << "Loading from   : " << arg_prev.getValue() << '\n';
    std::cout << "Transferring to: " << arg_next.getValue() << '\n';

    PCBDesign prev(arg_prev.getValue());
    PCBDesign next(arg_next.getValue());

    next.importFootprints(prev);
    next.importNodes(prev);
    next.save(arg_outp.getValue());

  } catch (TCLAP::ArgException& err) {
    std::cerr << "command line error: " << err.what() << "\n";
    return -1;
  } catch (LibSL::Errors::Fatal& err) {
    std::cerr << LibSL::CppHelpers::Console::red << "error: " << err.message() << LibSL::CppHelpers::Console::gray << "\n";
    return -2;
  } catch (std::exception& err) {
    std::cerr << "error: " << err.what() << "\n";
    return -3;
  }
  return 0;
}