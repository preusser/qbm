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
#ifndef STATEMENT_HPP
#define STATEMENT_HPP

#include "Decl.hpp"

#include <vector>
#include <memory>

class Context;
class Expression;

//- Abstract Base ------------------------------------------------------------
class Statement : public Decl {
protected:
  Statement() {}
public:
  virtual ~Statement() {}

public:
  virtual void execute(Context &ctx) const = 0;
};

//- Declarations  ------------------------------------------------------------
class Declaration : public Statement {
  std::string const                  m_name;
  std::shared_ptr<Expression const>  m_expr;

protected:
  Declaration(std::string                const &name,
	      std::shared_ptr<Expression const> expr)
    : m_name(name), m_expr(expr) {}
public:
  ~Declaration() {}

public:
  std::string const& name() const { return  m_name; }
  Expression  const& expr() const { return *m_expr; }
};

class ConstDecl : public Declaration {
public:
  ConstDecl(std::string const &name, std::shared_ptr<Expression const> value)
    : Declaration(name, value) {}
  ~ConstDecl();

public:
  Expression  const& value() const { return  expr(); }
  void dump(std::ostream &out) const;
  void execute(Context &ctx) const;
};

class ConfigDecl : public Declaration {
public:
  ConfigDecl(std::string const &name, std::shared_ptr<Expression const> width)
    : Declaration(name, width) {}
  ~ConfigDecl();

public:
  Expression  const& width() const { return  expr(); }
  void dump(std::ostream &out) const;
  void execute(Context &ctx) const;
};

class SignalDecl : public Declaration {
public:
  SignalDecl(std::string const &name, std::shared_ptr<Expression const> width)
    : Declaration(name, width) {}
  ~SignalDecl();

public:
  Expression  const& width() const { return  expr(); }
  void dump(std::ostream &out) const;
  void execute(Context &ctx) const;
};

//- Behavioral Equations -----------------------------------------------------
class Equation : public Statement {
  std::shared_ptr<Expression const>  m_lhs;
  std::shared_ptr<Expression const>  m_rhs;

public:
  Equation(std::shared_ptr<Expression const>  lhs,
	   std::shared_ptr<Expression const>  rhs)
    : m_lhs(lhs), m_rhs(rhs) {}
  ~Equation();

public:
  void dump(std::ostream &out) const;
  void execute(Context &ctx) const;
};

class InEquation : public Statement {
  std::shared_ptr<Expression const>  m_lhs;
  std::shared_ptr<Expression const>  m_rhs;

public:
  InEquation(std::shared_ptr<Expression const>  lhs,
	     std::shared_ptr<Expression const>  rhs)
    : m_lhs(lhs), m_rhs(rhs) {}
  ~InEquation();

public:
  void dump(std::ostream &out) const;
  void execute(Context &ctx) const;
};

//- Structural Instantiation -------------------------------------------------
class CompDecl;
class Instantiation : public Statement {
  std::string const  m_label;
  CompDecl    const &m_decl;

  std::vector<std::shared_ptr<Expression const>>  m_params;
  std::vector<std::shared_ptr<Expression const>>  m_connects;

public:
  Instantiation(std::string const &label, CompDecl const &decl)
    : m_label(label), m_decl(decl) {}
  ~Instantiation();

public:
  std::string const& label() const { return  m_label; }
  CompDecl    const& decl()  const { return  m_decl; }

public:
  void dump(std::ostream &out) const;

  //- Generics
public:
  void addParameter(std::shared_ptr<Expression const>  expr) {
    m_params.emplace_back(expr);
  }
  unsigned countParameters() const {
    return  m_params.size();
  }
  Expression const& getParameter(unsigned const  idx) const {
    return *m_params[idx];
  }

  //- Ports
public:
  void addConnection(std::shared_ptr<Expression const>  expr) {
    m_connects.emplace_back(expr);
  }
  Expression const& getConnection(unsigned const  idx) const {
    return *m_connects[idx];
  }

  //- Execution
public:
  void execute(Context &ctx) const;
};

//- Generate -----------------------------------------------------------------
class Generate : public Statement {
  std::string const                  m_var;
  std::shared_ptr<Expression const>  m_lo;
  std::shared_ptr<Expression const>  m_hi;
  std::vector<std::shared_ptr<Statement const>>  m_body;

public:
  Generate(std::string const                 &var,
	   std::shared_ptr<Expression const>  lo,
	   std::shared_ptr<Expression const>  hi)
    : m_var(var), m_lo(lo), m_hi(hi) {}
  ~Generate();

public:
  void dump(std::ostream &out) const;

public:
  void addStatement(std::shared_ptr<Statement const >  stmt) {
    m_body.emplace_back(stmt);
  }

public:
  void execute(Context &ctx) const;
};
#endif
