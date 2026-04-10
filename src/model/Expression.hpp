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
#ifndef EXPRESSION_HPP
#define EXPRESSION_HPP

#include <string>
#include <iostream>
#include <memory>
#include <array>

class ConstExpression;
class NameExpression;
class UniExpression;
class BiExpression;
class CondExpression;
class RangeExpression;
class ChooseExpression;

class Expression {
protected:
  Expression() {}
public:
  ~Expression() {}

public:
  class Visitor {
  protected:
    Visitor() {}
    ~Visitor() {}

  public:
    virtual void visit(ConstExpression  const &expr) = 0;
    virtual void visit(NameExpression   const &expr) = 0;
    virtual void visit(UniExpression    const &expr) = 0;
    virtual void visit(BiExpression     const &expr) = 0;
    virtual void visit(CondExpression   const &expr) = 0;
    virtual void visit(RangeExpression  const &expr) = 0;
    virtual void visit(ChooseExpression const &expr) = 0;
  };
  virtual void accept(Visitor &vis) const = 0;
};

class ConstExpression : public Expression {
  int const  m_val;
  int const  m_width;  // -1: implied minimum, >=0: explicit

public:
  ConstExpression(int const  val) : m_val(val), m_width(-1) {}
  ConstExpression(int const  val, int const  width) : m_val(val), m_width(width) {}
  ~ConstExpression();

public:
  int value() const { return  m_val; }
  int width() const { return  m_width; }

public:
  void accept(Visitor &vis) const;
};

class NameExpression : public Expression {
  std::string const  m_name;

public:
  NameExpression(std::string const &name) : m_name(name) {}
  ~NameExpression();

public:
  std::string const& name() const { return  m_name; }

public:
  void accept(Visitor &vis) const;
};

class UniExpression : public Expression {
public:
  enum class Op { NOT, NEG, LD };
  static inline constexpr std::array<char const*, 4>  OPS = {
    "~", "-", "ld ", "??"
  };

private:
  Op const  m_op;
  std::shared_ptr<Expression const>  m_arg;

public:
  UniExpression(Op const  op, std::shared_ptr<Expression const>  arg)
    : m_op(op), m_arg(arg) {}
  ~UniExpression();

public:
  Op op() const { return  m_op; }
  Expression const& arg() const { return *m_arg; }

public:
  void accept(Visitor &vis) const;
};

class BiExpression : public Expression {
public:
  enum class Op { AND, OR, XOR, ADD, SUB, MUL, DIV, MOD, POW, SEL, CAT };
  static inline constexpr std::array<char const*, 12>  OPS = {
    "&", "|", "^", "+", "-", "*", "/", "%", "**", "@", "#", "??"
  };

private:
  Op const  m_op;
  std::shared_ptr<Expression const>  m_left, m_right;

public:
  BiExpression(Op const  op,
	       std::shared_ptr<Expression const>  left,
	       std::shared_ptr<Expression const>  right) 
   : m_op(op), m_left(left), m_right(right) {}
  ~BiExpression();

public:
  Op op() const { return  m_op; }
  Expression const& lhs() const { return *m_left; }
  Expression const& rhs() const { return *m_right; }

public:
  void accept(Visitor &vis) const;
};

class CondExpression : public Expression {
  std::shared_ptr<Expression const>  m_cond, m_pos, m_neg;

public:
  CondExpression(std::shared_ptr<Expression const>  cond,
		 std::shared_ptr<Expression const>  pos,
		 std::shared_ptr<Expression const>  neg)
   : m_cond(cond), m_pos(pos), m_neg(neg) {}
  ~CondExpression();

public:
  Expression const& cond() const { return *m_cond; }
  Expression const& pos () const { return *m_pos; }
  Expression const& neg () const { return *m_neg; }

public:
  void accept(Visitor &vis) const;
};

class RangeExpression : public Expression {
  std::shared_ptr<Expression const>  m_base, m_left, m_right;

public:
  RangeExpression(std::shared_ptr<Expression const>  base,
		  std::shared_ptr<Expression const>  left,
		  std::shared_ptr<Expression const>  right)
   : m_base(base), m_left(left), m_right(right) {}
  ~RangeExpression();

public:
  Expression const& base () const { return *m_base; }
  Expression const& left () const { return *m_left; }
  Expression const& right() const { return *m_right; }

public:
  void accept(Visitor &vis) const;
};

class ChooseExpression : public Expression {
  std::shared_ptr<Expression const>  m_width, m_from;

public:
  ChooseExpression(std::shared_ptr<Expression const>  width,
		   std::shared_ptr<Expression const>  from)
    : m_width(width), m_from(from) {}
  ~ChooseExpression();

public:
  Expression const& width() const { return *m_width; }
  Expression const& from () const { return *m_from; }

public:
  void accept(Visitor &vis) const;
};

class ExpressionPrinter : public Expression::Visitor {
  std::ostream &m_out;
public:
  ExpressionPrinter(std::ostream &out) : m_out(out) {}
  ~ExpressionPrinter() {}

public:
  void visit(ConstExpression  const &expr);
  void visit(NameExpression   const &expr);
  void visit(UniExpression    const &expr);
  void visit(BiExpression     const &expr);
  void visit(CondExpression   const &expr);
  void visit(RangeExpression  const &expr);
  void visit(ChooseExpression const &expr);
};

inline std::ostream &operator<<(std::ostream &out, Expression const& expr) {
  ExpressionPrinter  printer(out);
  expr.accept(printer);
  return  out;
}
#endif
