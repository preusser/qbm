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
#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "Bus.hpp"
#include "Root.hpp"
#include "CompDecl.hpp"
#include "Statement.hpp"

#include <initializer_list>
#include <map>

class Expression;

class Context {
  Root     &m_root;
  Scope    &m_scope;
  unsigned  m_subcnt;

protected:
  std::map<std::string, int>  m_constants;
  std::map<std::string, Bus>  m_busses;

public:
  Context(Context const&) = delete;
  Context(Root &root, Scope &scope)
    : m_root(root), m_scope(scope), m_subcnt(0) {}
  Context(Root &root, Scope &scope, std::map<std::string, int> &&constants)
    : m_root(root), m_scope(scope), m_subcnt(0), m_constants(constants) {}

  Context(Context &parent, std::string const &name,
	  std::map<std::string, int> &&constants,
	  std::map<std::string, Bus> &&busses)
    : m_root(parent.root()), m_scope(parent.scope().createChild(name)), m_subcnt(0),
      m_constants(constants), m_busses(busses) {}
  ~Context() {}

public:
  Root&  root()  { return  m_root; }
  Scope& scope() { return  m_scope; }
  unsigned allocateSubscopeId() { return m_subcnt++; }

public:
  Bus allocateConfig(unsigned  width) { return  m_root.allocateConfig(width); }
  Bus allocateInput (unsigned  width) { return  m_root.allocateInput (width); }
  Bus allocateSignal(unsigned  width) { return  m_root.allocateSignal(width); }

public:
  void addClause(int const *beg, int const *end)         { m_root.addClause(beg, end); }
  void addClause(std::initializer_list<int> const  lits) { addClause(lits.begin(), lits.end()); }

public:
  void compile(std::string const &name, CompDecl const &comp) {
    std::cout << "Compiling " << name << " : " << comp.name() << " ..." << std::endl;
    comp.forAllStatements([this](Statement const &stmt) { stmt.execute(*this); });
  }

public:
  void defineConstant(std::string const &name, int val);
  int computeConstant(Expression  const &name) const;
  virtual int resolveConstant(std::string const &name) const;

  void registerConfig(std::string const &name, Bus const &bus);
  void registerSignal(std::string const &name, Bus const &bus);
  Bus computeBus(Expression  const &name);
  virtual Bus resolveBus(std::string const &name) const;
};

class InnerContext : public Context {
  Context const &m_parent;

public:
  InnerContext(Context &parent, std::string const &name)
    : Context(parent.root(), parent.scope().createChild(name)), m_parent(parent) {}
  ~InnerContext() {}

public:
  virtual int resolveConstant(std::string const &name) const;
  virtual Bus resolveBus(std::string const &name) const;
};
#endif
