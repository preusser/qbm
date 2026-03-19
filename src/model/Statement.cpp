/*****************************************************************************
 * This file is part of the QBM (Quantified Binary Matching) program.
 *
 * Copyright (C) 2016
 *      Thomas B. Preusser <thomas.preusser@utexas.edu>
 *****************************************************************************
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "Statement.hpp"

#include "CompDecl.hpp"
#include "Context.hpp"

#include <sstream>

ConstDecl::~ConstDecl() {}
void ConstDecl::dump(std::ostream &out) const {
  out << "constant " << name() << " = " << expr();
}
void ConstDecl::execute(Context &ctx) const {
  ctx.defineConstant(name(), ctx.computeConstant(expr()));
}

ConfigDecl::~ConfigDecl() {}
void ConfigDecl::dump(std::ostream &out) const {
  out << "config " << name() << '[' << width() << ']';
}
void ConfigDecl::execute(Context &ctx) const {
  ctx.registerConfig(name(), ctx.allocateConfig(ctx.computeConstant(width())));
}

SignalDecl::~SignalDecl() {}
void SignalDecl::dump(std::ostream &out) const {
  out << "signal " << name() << '[' << width() << ']';
}
void SignalDecl::execute(Context &ctx) const {
  ctx.registerSignal(name(), ctx.allocateSignal(ctx.computeConstant(width())));
}

Equation::~Equation() {}
void Equation::dump(std::ostream &out) const {
  out << *m_lhs << " = " << *m_rhs;
}
void Equation::execute(Context &ctx) const {
  Bus const  lhs = ctx.computeBus(*m_lhs);
  Bus const  rhs = ctx.computeBus(*m_rhs);
  for(unsigned  i = std::max(lhs.width(), rhs.width()); i-- > 0;) {
    int const  a = lhs[i];
    int const  b = rhs[i];
    ctx.addClause({ a, -b});
    ctx.addClause({-a,  b});
  }
}

Instantiation::~Instantiation() {}
void Instantiation::dump(std::ostream &out) const {
  out << m_label << " : " << m_decl.name();
  if(!m_params.empty()) {
    char const *sep = "<";
    for(auto const& ptr : m_params) {
      out << sep << *ptr;
      sep = ", ";
    }
    out << '>';
  }
  out << '(';
  char const *sep = "";
  for(auto const &ptr : m_connects) {
    out << sep << *ptr;
    sep = ", ";
  }
  out << ')';
}
void Instantiation::execute(Context &ctx) const {
  std::map<std::string, int>  params;
  { // Compute Generic Parameters
    unsigned const  n = m_params.size();
    if(n != m_decl.countParameters())  throw "Wrong number of parameters.";
    for(unsigned  i = 0; i < n; i++)  params[m_decl.getParameter(i).name()] = ctx.computeConstant(*m_params[i]);
  }
  std::map<std::string, Bus>  connects;
  { // Compute Generic Parameters
    unsigned const  n = m_connects.size();
    if(n != m_decl.countPorts())  throw "Wrong number of ports.";
    for(unsigned  i = 0; i < n; i++) {
      connects[m_decl.getPort(i).name()] = ctx.computeBus(*m_connects[i]);
    }
  }
  Context(ctx, m_label + '/', std::move(params), std::move(connects)).compile(m_label, m_decl);
}

Generate::~Generate() {}
void Generate::execute(Context &ctx) const {
  int const  lo = ctx.computeConstant(*m_lo);
  int const  hi = ctx.computeConstant(*m_hi);
  unsigned const  block = ctx.allocateSubscopeId();

  std::string const& var = m_var;
  for(int  i = lo; i <= hi; i++) {
    std::stringstream  name;
    name << "gen" << block << ':' << i << '.';
    InnerContext  local(ctx, name.str());
    local.defineConstant(var, i);
    for(std::shared_ptr<Statement const> const& stmt : m_body) {
      stmt->execute(local);
    }
  }
}
void Generate::dump(std::ostream &out) const {
  out << "for " << m_var << '=' << *m_lo << ".." << *m_hi << " generate" << std::endl;
  for(auto const &stmt : m_body)  stmt->dump(out);
  out << "end;";
}
