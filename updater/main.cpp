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

  std::string signature(sexpresso::Sexp& se,int child_id)
  {
    std::string s;
    for (int c = 0 ; c < child_id ; ++c) {
      if (se.getChild(c).isString()) {
        s = s + "_" + se.getChild(c).value.str;
      }
    }
    return s;
  }


  void findAttributes(sexpresso::Sexp& se,
                     sexpresso::Sexp&  parent,int child_id,
                     std::map<std::string,sexpresso::Sexp*>& _sig_to_node,
                     std::map<std::string,std::map<std::string,sexpresso::Sexp*> >& _all_attrs)
  {
    if (se.childCount() > 0) {
      if (se.getChild(0).isString()) {
        if (se.getChild(0).value.str == "at") {
          auto sig = signature(parent, child_id);
          _sig_to_node[sig]     = &parent;
          _all_attrs[sig]["at"] = &se;
        } else if (se.getChild(0).value.str == "justify") {
          LIBSL_TRACE;
          auto sig = signature(parent, child_id);
          std::cerr << sig << '\n';
          _sig_to_node[sig]     = &parent;
          _all_attrs[sig]["justify"] = &se;
        }
      }
    }
    for (int c = 0; c < se.childCount(); ++c) {
      if (se.getChild(c).isString()) {
        if (se.getChild(c).value.str == "hide") {
          auto sig = signature(se,c);
          _sig_to_node[sig]       = &se;
          _all_attrs[sig]["hide"] = &se.getChild(c);
        }
      } else {
        findAttributes(se.getChild(c), se,c, _sig_to_node, _all_attrs);
      }
    }
  }

public:

  PCBDesign(std::string fname)
  {
    std::ifstream     t(fname);
    std::stringstream buffer;
    buffer << t.rdbuf();
    m_Root = sexpresso::parse(buffer.str());
    extractFootprints(m_Root);
  }

  void importAttributes(PCBDesign& source)
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
        *D->second = *sse;
#if 0
        // find positions in the source footprint
        std::map<std::string, std::map<std::string, sexpresso::Sexp*> > srcs;
        std::map<std::string, sexpresso::Sexp*> src_sig_to_node;
        source.findAttributes(*sse,source.root(),0,src_sig_to_node,srcs);
        // find positions in this footprint
        std::map<std::string, std::map<std::string, sexpresso::Sexp*> > dests;
        std::map<std::string, sexpresso::Sexp*> dst_sig_to_node;
        findAttributes(*D->second,m_Root,0, dst_sig_to_node,dests);
        std::cerr << "===== footprints " << sfp << " <-> " << D->first << '\n';
        // match all and override
        for (auto dpos : dests) { // go through all destination signatures
          auto S = srcs.find(dpos.first); // search in source signatures
          if (S != srcs.end()) { // match!
            // replace nodes with
            for (auto attr : S->second) {
              if (dpos.second.count(attr.first)) {
                // replace
                (*dpos.second.at(attr.first)) = (*attr.second);
              } else {
                // add
                LIBSL_TRACE;
                // -> find parent in destination
                auto D = dst_sig_to_node.find(dpos.first);
                sl_assert(D != dst_sig_to_node.end());
                LIBSL_TRACE;
                D->second->addChild(*attr.second);
              }
            }
          }
        }
#endif
      }
    }
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

int main(int,const char**)
{

  PCBDesign prev("prev.kicad_pcb");
  PCBDesign next("test.kicad_pcb");

  next.importAttributes(prev);
  next.importNodes(prev);
  next.save("out.kicad_pcb");

  return 0;
}