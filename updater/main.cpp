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
                     // TODO: std::map<std::string,sexpresso::Sexp*> _sig_to_node,
                     std::map<std::string,std::map<std::string,sexpresso::Sexp*> >& _all_attrs)
  {
    if (se.childCount() > 0) {
      if (se.getChild(0).isString()) {
        if (se.getChild(0).value.str == "at") {
          auto sig = signature(parent, child_id);
          // _sig_to_node[sig] = &se;
          _all_attrs[sig]["at"] = &se;
        }
      }
    }
    for (int c = 0; c < se.childCount(); ++c) {
      if (se.getChild(c).isString()) {
        if (se.getChild(c).value.str == "hide") {
          auto sig = signature(se,c);
          // _sig_to_node[sig] = &se;
          _all_attrs[sig]["hide"] = &se.getChild(c);
        }
      } else {
        findAttributes(se.getChild(c), se,c, _all_attrs);
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
    for (auto [sfp,sse] : source.footprintsByRef()) {
      // find corresponding footprint in this design
      auto D = m_FootprintsByRef.find(sfp);
      if (D != m_FootprintsByRef.end()) { // match!
        // find positions in the source footprint
        std::map<std::string, std::map<std::string, sexpresso::Sexp*> > srcs;
        source.findAttributes(*sse,source.root(),0,srcs);
        // find positions in this footprint
        std::map<std::string, std::map<std::string, sexpresso::Sexp*> > dests;
        findAttributes(*D->second,m_Root,0,dests);
        std::cerr << "===== footprints " << sfp << " <-> " << D->first << '\n';
        // match all and override
        for (auto dpos : dests) { // go through all destination signatures
          auto S = srcs.find(dpos.first); // search in source signatures
          if (S != srcs.end()) { // match!
            // replace nodes with
            for (auto attr : S->second) {
              if (dpos.second.count(attr.first)) {
                (*dpos.second.at(attr.first)) = (*attr.second);
              } else {
                // TODO (add attr)
                LIBSL_TRACE;
              }
            }
          }
        }
      }
    }
  }

  void importNodes(PCBDesign& source)
  {
    // get the kicad_pcb root
    sl_assert(m_Root.childCount() > 0);
    sexpresso::Sexp& d_kicad = m_Root.getChild(0);
    sl_assert(source.root().childCount() > 0);
    sexpresso::Sexp& s_kicad = source.root().getChild(0);
    // footprints
    for (auto [sfp, sse] : source.footprintsByRef()) {
      // find corresponding footprint in this design
      auto D = m_FootprintsByRef.find(sfp);
      if (D == m_FootprintsByRef.end()) { // not found
        // add back the footprint // NOTE TODO FIXME well maybe this was trully removed? add back only it is has no ref from skidl?
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
  PCBDesign next("next.kicad_pcb");
  
  next.importAttributes(prev);
  next.importNodes(prev);
  next.save("test.kicad_pcb");
  
  return 0;
}