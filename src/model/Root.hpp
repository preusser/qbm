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
#ifndef ROOT_HPP
#define ROOT_HPP

#include "Bus.hpp"
#include "Result.hpp"
#include "Scope.hpp"

#include <vector>
#include <functional>
#include <algorithm>

class CompDecl;
class Root {
public:
  static constexpr int  FIRST_CONFIG =          2;
  static constexpr int  FIRST_INPUT  = 0x3FFF0000;
  static constexpr int  FIRST_SIGNAL = 0x40000000;

private:
  Scope  m_top;

  std::vector<int>  m_clauses;
  int  m_confignxt;
  int  m_inputnxt;
  int  m_signalnxt;

  Result  m_res;

public:
  Root(CompDecl const &decl, std::vector<int> const &generics);
  ~Root() {}

public:
  Bus allocateConfig(unsigned  width);
  Bus allocateInput (unsigned  width);
  Bus allocateSignal(unsigned  width);

public:
  void print(std::ostream &out, Bus const &bus) const;
  void addClause(int const *beg, int const *end);
  void dumpClauses(std::ostream &out) const;

private:
  std::function<int(int)> varCompactor() const;

public:
  void dumpQDimacs(std::ostream &out) const;
  Result solve();
  bool resolve(int const  v) const {
    return  std::binary_search(m_clauses.begin(), m_clauses.end(), v);
  }
  void printConfig(std::ostream &out) const;
};
#endif
