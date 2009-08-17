// Filename: p3dFileParams.h
// Created by:  drose (23Jun09)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#ifndef P3DFILEPARAMS_H
#define P3DFILEPARAMS_H

#include "p3d_plugin_common.h"
#include "get_tinyxml.h"
#include <vector>

////////////////////////////////////////////////////////////////////
//       Class : P3DFileParams
// Description : Encapsulates the file parameters: the p3d_filename,
//               and extra tokens.
////////////////////////////////////////////////////////////////////
class P3DFileParams {
public:
  P3DFileParams();
  P3DFileParams(const P3DFileParams &copy);
  void operator = (const P3DFileParams &other);

  void set_p3d_filename(const string &p3d_filename);
  void set_tokens(const P3D_token tokens[], size_t num_tokens);
  void set_args(int argc, const char *argv[]);

  inline const string &get_p3d_filename() const;
  string lookup_token(const string &keyword) const;
  bool has_token(const string &keyword) const;

  TiXmlElement *make_xml();

private:
  class Token {
  public:
    string _keyword;
    string _value;
  };
  typedef vector<Token> Tokens;
  typedef vector<string> Args;

  string _p3d_filename;
  Tokens _tokens;
  Args _args;
};

#include "p3dFileParams.I"

#endif
