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
#include "Expression.hpp"
#include <array>

ConstExpression::~ConstExpression() {}
void ConstExpression::accept(Visitor &vis) const { vis.visit(*this); }

NameExpression::~NameExpression() {}
void NameExpression::accept(Visitor &vis) const { vis.visit(*this); }

UniExpression::~UniExpression() {}
void UniExpression::accept(Visitor &vis) const { vis.visit(*this); }
std::array<char const *const, 4>  UniExpression::OPS = {
  "~", "-", "ld ", "??"
};

BiExpression::~BiExpression() {}
void BiExpression::accept(Visitor &vis) const { vis.visit(*this); }
std::array<char const *const, 12>  BiExpression::OPS = {
  "&", "|", "^", "+", "-", "*", "/", "%", "**", "@", "#", "??"
};

CondExpression::~CondExpression() {}
void CondExpression::accept(Visitor &vis) const { vis.visit(*this); }

RangeExpression::~RangeExpression() {}
void RangeExpression::accept(Visitor &vis) const { vis.visit(*this); }

ChooseExpression::~ChooseExpression() {}
void ChooseExpression::accept(Visitor &vis) const { vis.visit(*this); }


void ExpressionPrinter::visit(ConstExpression const &expr) {
  m_out << expr.value();
}
void ExpressionPrinter::visit(NameExpression const &expr) {
  m_out << expr.name();
}
void ExpressionPrinter::visit(UniExpression const &expr) {
  m_out << UniExpression::OPS[std::min((size_t)expr.op(), UniExpression::OPS.size()-1)];
  expr.arg().accept(*this);
}
void ExpressionPrinter::visit(BiExpression const &expr) {
  m_out << '(';
  expr.lhs().accept(*this);
  m_out << ')' << BiExpression::OPS[std::min((size_t)expr.op(), BiExpression::OPS.size()-1)] << '(';
  expr.rhs().accept(*this);
  m_out << ')';
}
void ExpressionPrinter::visit(CondExpression const &expr) {
  expr.cond().accept(*this);
  m_out << "? ";
  expr.pos().accept(*this);
  m_out << " : ";
  expr.neg().accept(*this);
}
void ExpressionPrinter::visit(RangeExpression const &expr) {
  expr.base().accept(*this);
  m_out << '[';
  expr.left().accept(*this);
  m_out << ':';
  expr.right().accept(*this);
  m_out << ']';
}
void ExpressionPrinter::visit(ChooseExpression const &expr) {
  m_out << "CHOOSE<";
  expr.width().accept(*this);
  m_out << ">(";
  expr.from().accept(*this);
  m_out << ')';
}
