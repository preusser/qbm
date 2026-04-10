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
#ifndef NODE_HPP
#define NODE_HPP

class Node {
public:
  static constexpr int  TOP =  1;
  static constexpr int  BOT = -1;

private:
  unsigned  m_val;

public:
  constexpr Node()                : m_val(BOT)     {}
  constexpr Node(Node const &o)   : m_val(o.m_val) {}
  constexpr Node(int  const  val) : m_val(val)     {}

public:
  operator int() const { return  m_val; }
};
#endif
