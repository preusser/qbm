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
#ifndef BUS_HPP
#define BUS_HPP

#include "Node.hpp"

#include <memory>

class Bus {
  unsigned                     m_width;
  std::shared_ptr<Node const[]>  m_nodes;

private:
  static unsigned computeBitWidth(unsigned  val) {
    unsigned  width = 0;
    for(unsigned  v = val; v != 0; v >>= 1)  width++;
    return  width;
  }
public:
  Bus() : m_width(0) {}
  Bus(unsigned  val) : Bus(val, computeBitWidth(val)) {}
  Bus(unsigned  val, unsigned  width) : m_width(width) {
    if(width > 0) {
      Node *const  bus = new Node[width];
      for(unsigned  i = 0; i < width; i++) {
	bus[i] = val&1? Node::TOP : Node::BOT;
	val >>= 1;
      }
      m_nodes.reset(bus);
    }
  }
  Bus(unsigned const  width, Node const* nodes)
    : m_width(width), m_nodes(nodes) {}
  Bus(unsigned const  width, std::shared_ptr<Node const[]> nodes)
    : m_width(width), m_nodes(std::move(nodes)) {}
  ~Bus() {}

public:
  unsigned width() const { return  m_width; }

  Node operator[](unsigned const  ofs) const {
    return (ofs >= m_width)? Node(Node::BOT) : m_nodes[ofs];
  }
  Bus operator()(unsigned const  beg, unsigned const  end) const {
    if(end < beg)  return  Bus();

    Node const *const  src = m_nodes.get();
    unsigned    const  len = end-beg+1;
    Node       *const  dst = new Node[len];
    if(end < m_width)  std::copy(src+beg, src+end+1, dst);
    else if(beg < m_width) {
      std::copy(src+beg, src+m_width, dst);
      std::fill(dst+(m_width-beg), dst+len, Node(Node::BOT));
    }
    else  std::fill(dst, dst+len, Node(Node::BOT));
    return  Bus(len, dst);
  }
  Bus operator~() const {
    Node const *const  src = m_nodes.get();
    Node       *const  dst = new Node[m_width];
    for(unsigned  i = 0; i < m_width; i++)  dst[i] = -src[i];
    return  Bus(m_width, dst);
  }
  Bus operator,(Bus const &o) const {
    Node *const  dst = new Node[m_width + o.m_width];
    std::copy(o.m_nodes.get(), o.m_nodes.get()+o.m_width, dst);
    std::copy(m_nodes.get(), m_nodes.get()+m_width, dst+o.m_width);
    return  Bus(m_width+o.m_width, dst);
  }
};
#endif
