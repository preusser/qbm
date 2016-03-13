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
#include "Root.hpp"

#include "Quantor.hpp"

Bus Root::allocateConfig(unsigned  width) {
  Node *const  nodes = new Node[width];
  for(unsigned  i = 0; i < width; i++) {
    nodes[i] = m_confignxt++;
  }
  return  Bus(width, nodes);
}

Bus Root::allocateInput (unsigned  width) {
  Node *const  nodes = new Node[width];
  for(unsigned  i = 0; i < width; i++) {
    nodes[i] = m_inputnxt++;
  }
  return  Bus(width, nodes);
}

Bus Root::allocateSignal(unsigned  width) {
  Node *const  nodes = new Node[width];
  for(unsigned  i = 0; i < width; i++) {
    nodes[i] = m_signalnxt++;
  }
  return  Bus(width, nodes);
}

void Root::addClause(int const *beg, int const *end) {
  auto const  size = m_clauses.size();
  while(beg < end) {
    int const v = *beg++;
    switch(v) {
    default:
      m_clauses.push_back(v);
    case Node::BOT:
      continue;
    case Node::TOP:
      // Rollback to remove already satisfied clause
      m_clauses.resize(size);
      return;
    }
  }
  m_clauses.push_back(0);
}

void Root::dumpClauses(std::ostream &out) const {
  for(int  v : m_clauses) {
    if(v == 0)  out << std::endl;
    else {
      if(v < 0) {
	out << '~';
	v = -v;
      }
      char  c = 'X'; // this should not stick!
      if(v >= FIRST_SIGNAL) {
	c = 'n';
	v -= FIRST_SIGNAL;
      }
      else if(v >= FIRST_INPUT) {
	c = 'i';
	v -= FIRST_INPUT;
      }
      else if(v >= FIRST_CONFIG) {
	c = 'c';
	v -= FIRST_CONFIG;
      }
      out << c << v << ' ';
    }
  }
}

void Root::solve() {
  qbm::Quantor  q;

  q.scope(QUANTOR_EXISTENTIAL_VARIABLE_TYPE);
  for(int  i = FIRST_CONFIG; i < m_confignxt; i++)  q.add(i);
  q.add(0);

  q.scope(QUANTOR_UNIVERSAL_VARIABLE_TYPE);
  for(int  i = FIRST_INPUT; i < m_inputnxt; i++)  q.add(i);
  q.add(0);

  q.scope(QUANTOR_EXISTENTIAL_VARIABLE_TYPE);
  for(int  i = FIRST_SIGNAL; i < m_signalnxt; i++)  q.add(i);
  q.add(0);

  for(int lit : m_clauses) q.add(lit);
  Result const res = q.sat();
  std::cout << res << std::endl;
  if(res) {
    int const *assign = q.assignment();
    while(*assign) {
      std::cout << (*assign++) << ' ';
    }
  }
}
