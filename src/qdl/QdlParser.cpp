#line 33 "QdlParser.ypp"

# include "QdlParser.hpp"

# include "Expression.hpp"
# include "Statement.hpp"
# include "CompDecl.hpp"
# include "Lib.hpp"
# include <cctype>
# include <functional>
# include <memory>
# include <limits>
# include <fstream>
# include <sstream>

  QdlParser::QdlParser(std::istream                                 &in,
		       std::unordered_map<std::string, std::string>&&defines,
		       Lib                                          &lib)
    : m_lib(lib), m_newline(true), m_defines(defines) {
    m_sources.emplace(&in, [](std::istream*){});
    try {
      parse();
    }
    catch(...) {
      if(!m_sources.empty()) {
	std::string  line;
	getline(*m_sources.top(), line);
	std::cerr << "@\"" + line + '"' << std::endl;
      }
      throw;
    }
  }
  QdlParser::~QdlParser() {}

  void QdlParser::error(std::string  msg) {
    if(!m_sources.empty()) {
      std::string  line;
      getline(*m_sources.top(), line);
      msg += " before \"" + line + '"';
    }
    throw  msg;
  }

  class SVal {
    class Box {
    protected:
      Box() {}
      ~Box() {}
    };

    class Name : public Box {
      std::string const  m_val;
    public:
      Name(std::string const &val) : m_val(val) {}
      ~Name() {}
    public:
      std::string const& value() const { return  m_val; }
    };

    class Number : public Box {
      unsigned const  m_val;
    public:
      Number(unsigned const  val) : m_val(val) {}
      ~Number() {}
    public:
      unsigned value() const { return  m_val; }
    };

    class Expr : public Box {
      std::shared_ptr<Expression const> const  m_val;
    public:
      Expr(std::shared_ptr<Expression const> const &val) : m_val(val) {}
      ~Expr() {}
    public:
      std::shared_ptr<Expression const> const& value() const { return  m_val; }
    };

    class Bus : public Name {
      std::shared_ptr<Expression const> const  m_width;
    public:
      Bus(std::string const &name,
	  std::shared_ptr<Expression const> const &width)
	: Name(name), m_width(width) {}
      ~Bus() {}
    public:
      std::shared_ptr<Expression const> const& width() const { return  m_width; }
    };

    class Stmt : public Box {
      std::shared_ptr<Statement> const  m_val;
    public:
      Stmt(std::shared_ptr<Statement> const &val) : m_val(val) {}
      ~Stmt() {}
    public:
      std::shared_ptr<Statement> const& value() const { return  m_val; }
    };

    class Comp : public Box {
      CompDecl &m_val;
    public:
      Comp(CompDecl &val) : m_val(val) {}
      ~Comp() {}
    public:
      CompDecl& value() { return  m_val; }
    };

    std::shared_ptr<Box>  contents;

  public:
    SVal() {}
    ~SVal() {}

    // Setting
  public:
    SVal& operator=(std::string const& val) {
      contents.reset(new Name(val));
      return *this;
    }
    SVal& operator=(unsigned const  val) {
      contents.reset(new Number(val));
      return *this;
    }
    SVal& operator=(std::shared_ptr<Expression const> const &val) {
      contents.reset(new Expr(val));
      return *this;
    }
    SVal& operator=(std::shared_ptr<Statement> const &val) {
      contents.reset(new Stmt(val));
      return *this;
    }
    SVal& operator=(CompDecl &val) {
      contents.reset(new Comp(val));
      return *this;
    }
    SVal& makeBus(std::string                       const &name,
		  std::shared_ptr<Expression const> const &width) {
      contents.reset(new Bus(name, width));
      return *this;
    }

    // Querying
  public:
    std::string const& name() const {
      return  static_cast<Name&>(*contents).value();
    }
    unsigned number() const {
      return  static_cast<Number&>(*contents).value();
    }
    std::shared_ptr<Expression const> const& expr() const {
      return  static_cast<Expr&>(*contents).value();
    }
    std::shared_ptr<Statement> const& stmt() const {
      return  static_cast<Stmt&>(*contents).value();
    }
    CompDecl& comp() const {
      return  static_cast<Comp&>(*contents).value();
    }
    std::shared_ptr<Expression const> const& width() const {
      return  static_cast<Bus&>(*contents).width();
    }
  };

  unsigned QdlParser::nextToken(YYSVal &sval) {
    bool  newline = m_newline;
    m_newline = false;

    std::istream *in = m_sources.top().get();
    while(true) {
      int const  c = in->get();

      // Filter out Operators
      switch(c) {
      case EOF:
	m_sources.pop();
	if(m_sources.empty())  return  0;
	in = m_sources.top().get();
	continue;

      case '.':
	if(in->peek() == '.') {
	  in->ignore();
	  return  THROUGH;
	}
	goto  err;
	
      case '-':
	if(in->peek() == '>') {
	  in->ignore();
	  return  MAPSTO;
	}
	return  c;

      case '*':
	if(in->peek() == '*') {
	  in->ignore();
	  return  POWER;
	}
	return  c;

      case '/':
	if(in->peek() == '/') {
	  in->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	  newline = true;
	  continue;
	}
	return  c;

      case '!':
  if(in->peek() == '=') {
    in->ignore();
    return  NOTEQ;
  }
  goto  err;

      case '<':
      case '>':
      case '(':
      case ')':
      case '[':
      case ']':
      case '=':
      case ',':
      case ';':
      case '~':
      case '&':
      case '|':
      case '^':
      case '+':
      case '%':
      case ':':
      case '?':
      case '#':
	return  c;

      case '\'': // check for directives
	if(!newline)  goto  err;
	std::string   line;
	getline(*in, line, '\n');

	if(line.compare(0, 7, "include") == 0) {
	  std::unique_ptr<char[]>  filename(new char[line.size()]);
	  int  end = -1;

	  sscanf(line.c_str()+7, " \"%[^\"]\" %n", filename.get(), &end);
	  if((size_t)(end+7) == line.size()) {
	    m_sources.emplace(in = new std::ifstream(filename.get()),
			      std::default_delete<std::istream>());
	    continue;
	  }
	}
	else if(line.compare(0, 6, "define") == 0) {
	  std::unique_ptr<char[]>  ident(new char[line.size()]);
	  char const *beg = line.c_str() + 6;
	  int end = -1;

	  sscanf(beg, " %s %n", ident.get(), &end);
	  m_defines.emplace(ident.get(), beg+end);
	  continue;
	}
	else if(line.compare(0, 5, "undef") == 0) {
	  std::unique_ptr<char[]>  ident(new char[line.size()]);
	  int end = -1;

	  sscanf(line.c_str()+5, " %s %n", ident.get(), &end);
	  if((size_t)(end+5) == line.size()) {
	    m_defines.erase(ident.get());
	    continue;
	  }
	}
	throw "Malformed directive: " + line;
      }

      // Skip Space
      if(isspace(c)) {
	newline = c == '\n';
	continue;
      }

      // Keywords and Identifiers
      if(isalpha(c) || (c == '_')) {
	std::string  w(1, (char)c);
	while(true) {
	  int const  cc = in->peek();
	  if(!isalnum(cc) && (cc != '_'))  break;
	  w += (char)in->get();
	}
	if(w == "component")  return  COMPONENT;
	if(w == "constant")   return  CONSTANT;
	if(w == "config")     return  CONFIG;
	if(w == "CHOOSE")     return  CHOOSE;
	if(w == "end")        return  END;
	if(w == "for")        return  FOR;
	if(w == "generate")   return  GENERATE;
	if(w == "ld")	      return  LD;
	if(w == "signal")     return  SIGNAL;

	auto const  it = m_defines.find(w);
	if(it != m_defines.end()) {
	  // Temporarily remove this macro definition to counter recursions
	  std::pair<std::string, std::string>  cap = *it;
	  m_sources.emplace(in = new std::stringstream(it->second),
			    [this, cap](std::istream *is) {
			      m_defines.emplace(cap.first, cap.second);
			      delete  is;
			    });
	  m_defines.erase(it);
	  continue;
	}

	sval = w;
	return  IDENT;
      }

      // Numbers
      if(isdigit(c)) {
	unsigned  v = c - '0';
	while(true) {
	  if(!isdigit(in->peek()))  break;
	  v = 10*v + (in->get() - '0');
	}
	sval = v;
	return  NUMBER;
      }

    err:
      error(std::string("Illegal Character: '") + (char)c + "'");
    }
  } // nextToken()

#line 330 "QdlParser.cpp"
#include <vector>
class QdlParser::YYStack {
  class Ele {
  public:
    unsigned short  state;
    YYSVal          sval;

  public:
    Ele(unsigned short _state, YYSVal const& _sval)
     : state(_state), sval(_sval) {}
    Ele(unsigned short _state)
     : state(_state) {}
  };
  typedef std::vector<Ele>  Stack;
  Stack  stack;

public:
  YYStack() {}
  ~YYStack() {}

public:
  void push(unsigned short state) {
    stack.push_back(Ele(state));
  }
  void push(unsigned short state, YYSVal const& sval) {
    stack.push_back(Ele(state, sval));
  }
  void pop(unsigned cnt) {
    Stack::iterator  it = stack.end();
    stack.erase(it-cnt, it);
  }

  YYSVal& operator[](unsigned idx) { return  (stack.rbegin()+idx)->sval; }
  unsigned short operator*() const { return  stack.back().state; }
};

char const *const  QdlParser::yyterms[] = { "EOF", 
"POWER", "NUMBER", "IDENT", "CHOOSE", "COMPONENT", "CONFIG", "CONSTANT", "END",
"FOR", "GENERATE", "LD", "MAPSTO", "NOTEQ", "SIGNAL", "THROUGH", "'<'",
"','", "'('", "'>'", "')'", "';'", "'='", "':'", "'['",
"']'", "'~'", "'+'", "'-'", "'#'", "'*'", "'/'", "'%'",
"'&'", "'|'", "'^'", "'?'", };
unsigned short const  QdlParser::yyintern[] = {
     0,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,     29,    271,     32,     33,    271,
    18,     20,     30,     27,     17,     28,    271,     31,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,     23,     21,     16,     22,     19,     36,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,     24,    271,     25,     35,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,     34,    271,     26,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
   271,    271,    271,    271,    271,    271,    271,    271,
     1,      2,      3,      4,      5,      6,      7,      8,
     9,     10,     11,     12,     13,     14,     15, };

#ifdef TRACE
char const *const  QdlParser::yyrules[] = {
"   0: [ 0] $        -> comp_decl",
"   1: [ 2] comp_dec -> COMPONENT IDENT",
"   2: [ 2] comp_dec -> comp_decl_start '<' IDENT",
"   3: [ 2] comp_dec -> comp_decl_gen ',' IDENT",
"   4: [ 0] comp_dec -> comp_decl_start '(' bus",
"   5: [ 0] comp_dec -> comp_decl_gen '>' '(' bus",
"   6: [ 0] comp_dec -> comp_decl_in ',' bus",
"   7: [ 3] comp_dec -> comp_decl_in MAPSTO bus",
"   8: [ 0] comp_dec -> comp_decl_out ',' bus",
"   9: [ 0] comp_dec -> comp_decl_out ')'",
"  10: [ 0] comp_dec -> comp_decl_body stmt ';'",
"  11: [ 0] comp_dec -> comp_decl_body END ';'",
"  12: [ 0] comp_dec -> comp_decl comp_decl_body END ';'",
"  13: [ 0] stmt     -> CONSTANT IDENT '=' expr",
"  14: [ 3] stmt     -> CONFIG bus",
"  15: [ 3] stmt     -> SIGNAL bus",
"  16: [ 0] stmt     -> expr '=' expr",
"  17: [ 3] stmt     -> expr NOTEQ expr",
"  18: [ 0] stmt     -> inst_out ')'",
"  19: [ 3] stmt     -> generate END",
"  20: [ 2] inst_sta -> IDENT ':' IDENT",
"  21: [ 0] inst_gen -> inst_start '<' expr",
"  22: [ 0] inst_gen -> inst_gen ',' expr",
"  23: [ 0] inst_in  -> inst_start '(' expr",
"  24: [ 0] inst_in  -> inst_gen '>' '(' expr",
"  25: [ 0] inst_in  -> inst_in ',' expr",
"  26: [ 3] inst_out -> inst_in MAPSTO expr",
"  27: [ 0] inst_out -> inst_out ',' expr",
"  28: [ 3] generate -> FOR IDENT '=' expr THROUGH expr GENERATE",
"  29: [ 0] generate -> generate stmt ';'",
"  30: [ 2] bus      -> IDENT",
"  31: [ 0] bus      -> IDENT '[' expr ']'",
"  32: [ 2] ex_atom  -> NUMBER",
"  33: [ 2] ex_atom  -> IDENT",
"  34: [ 0] ex_atom  -> '~' ex_atom",
"  35: [ 0] ex_atom  -> '+' ex_atom",
"  36: [ 0] ex_atom  -> '-' ex_atom",
"  37: [ 3] ex_atom  -> LD ex_atom",
"  38: [ 0] ex_atom  -> CHOOSE '<' expr '>' '(' expr ')'",
"  39: [ 0] ex_atom  -> '(' expr ')'",
"  40: [ 0] ex_atom  -> ex_atom '[' expr ']'",
"  41: [ 0] ex_atom  -> ex_atom '[' expr ':' expr ']'",
"  42: [ 1] ex_atom  -> ex_atom POWER ex_atom",
"  43: [ 0] ex_cat   -> ex_atom",
"  44: [ 0] ex_cat   -> ex_cat '#' ex_atom",
"  45: [ 0] ex_mult  -> ex_cat",
"  46: [ 0] ex_mult  -> ex_mult '*' ex_cat",
"  47: [ 0] ex_mult  -> ex_mult '/' ex_cat",
"  48: [ 0] ex_mult  -> ex_mult '%' ex_cat",
"  49: [ 0] ex_logic -> ex_mult",
"  50: [ 0] ex_logic -> ex_logic '&' ex_mult",
"  51: [ 0] ex_logic -> ex_logic '|' ex_mult",
"  52: [ 0] ex_logic -> ex_logic '^' ex_mult",
"  53: [ 0] ex_add   -> ex_logic",
"  54: [ 0] ex_add   -> ex_add '+' ex_logic",
"  55: [ 0] ex_add   -> ex_add '-' ex_logic",
"  56: [ 0] expr     -> ex_add",
"  57: [ 0] expr     -> ex_add '?' ex_add ':' expr",
};
#endif
unsigned short const QdlParser::yylength[] = {
     1,      2,      3,      3,      3,      4,      3,      3,
     3,      2,      3,      3,      4,      4,      2,      2,
     3,      3,      2,      2,      3,      3,      3,      3,
     4,      3,      3,      3,      7,      3,      1,      4,
     1,      1,      2,      2,      2,      2,      7,      3,
     4,      6,      3,      1,      3,      1,      3,      3,
     3,      1,      3,      3,      3,      1,      3,      3,
     1,      5, };
unsigned short const QdlParser::yylhs   [] = {
   (unsigned short)~0u,      1,      2,      2,      3,      3,      3,      5,
     5,      6,      6,      0,      0,      7,      7,      7,
     7,      7,      7,      7,     11,     12,     12,     13,
    13,     13,      9,      9,     10,     10,      4,      4,
    14,     14,     14,     14,     14,     14,     14,     14,
    14,     14,     14,     15,     15,     16,     16,     16,
    16,     17,     17,     17,     17,     18,     18,     18,
     8,      8, };

unsigned short const  QdlParser::yygoto  [][19] = {
{      6,      5,      4,      3,      0,      2,      1,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,     21,     20,     19,     18,     17,     16,     15,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      5,      4,      3,      0,      2,      9,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,     21,     20,     19,     18,     17,     16,     15,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,     82,     20,     19,     18,     17,     16,     15,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,     65,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     54,      0,      0,      0,      0,  },
{      0,      0,      0,      0,     49,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     47,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     46,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     45,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     35,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     44,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     39,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     42,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     52,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     57,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     59,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     64,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     67,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     70,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     78,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     77,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     81,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     88,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     87,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     93,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     92,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     97,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,     96,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     99,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,    105,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,    104,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,    103,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,     13,    111,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,     13,    110,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,     13,    109,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,     13,     12,    119,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,     13,     12,    118,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     14,     13,     12,     11,    115,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,    117,      0,      0,      0,      0,      0,     14,     13,     12,     11,     10,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,    122,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,    127,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,    132,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,    131,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,    135,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
};

signed short const  QdlParser::yyaction[][37] = {
{      0,      0,      0,      0,      0,      7,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     23,     24,      0,     25,     26,    136,     28,      0,     29,      0,      0,     30,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    133,      0,      0,    134,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    129,      0,      0,      0,      0,    130,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    124,      0,    125,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    120,      0,    121,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      1,      0,      0,      0,      0,      7,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      8,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -1,      0,     -1,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     23,     24,      0,     25,     26,     27,     28,      0,     29,      0,      0,     30,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -56,      0,    -56,    -56,      0,    -56,      0,    -56,      0,    -56,    -56,    -56,    -56,    -56,      0,    -56,      0,    112,    113,      0,      0,      0,      0,      0,      0,      0,    114,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -53,      0,    -53,    -53,      0,    -53,      0,    -53,      0,    -53,    -53,    -53,    -53,    -53,      0,    -53,      0,    -53,    -53,      0,      0,      0,      0,    106,    107,    108,    -53,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -49,      0,    -49,    -49,      0,    -49,      0,    -49,      0,    -49,    -49,    -49,    -49,    -49,      0,    -49,      0,    -49,    -49,      0,    100,    101,    102,    -49,    -49,    -49,    -49,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -45,      0,    -45,    -45,      0,    -45,      0,    -45,      0,    -45,    -45,    -45,    -45,    -45,      0,    -45,      0,    -45,    -45,     98,    -45,    -45,    -45,    -45,    -45,    -45,    -45,  },
{      0,     37,      0,      0,      0,      0,      0,      0,      0,      0,    -43,      0,    -43,    -43,      0,    -43,      0,    -43,      0,    -43,    -43,    -43,    -43,    -43,     38,    -43,      0,    -43,    -43,    -43,    -43,    -43,    -43,    -43,    -43,    -43,    -43,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     94,      0,      0,      0,      0,     95,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     89,      0,     90,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     85,      0,     86,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     23,     24,      0,     25,     26,     83,     28,      0,     29,      0,      0,     30,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     79,      0,      0,     80,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     75,      0,      0,      0,      0,      0,      0,      0,      0,     76,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     74,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,    -32,      0,      0,      0,      0,      0,      0,      0,      0,    -32,      0,    -32,    -32,      0,    -32,      0,    -32,      0,    -32,    -32,    -32,    -32,    -32,    -32,    -32,      0,    -32,    -32,    -32,    -32,    -32,    -32,    -32,    -32,    -32,    -32,  },
{      0,    -33,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -33,      0,      0,      0,      0,      0,      0,      0,      0,    -33,     72,    -33,      0,      0,    -33,    -33,    -33,    -33,    -33,    -33,    -33,    -33,    -33,    -33,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     66,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     62,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     61,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     55,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,     37,      0,      0,      0,      0,      0,      0,      0,      0,    -36,      0,    -36,    -36,      0,    -36,      0,    -36,      0,    -36,    -36,    -36,    -36,    -36,     38,    -36,      0,    -36,    -36,    -36,    -36,    -36,    -36,    -36,    -36,    -36,    -36,  },
{      0,    -33,      0,      0,      0,      0,      0,      0,      0,      0,    -33,      0,    -33,    -33,      0,    -33,      0,    -33,      0,    -33,    -33,    -33,    -33,    -33,    -33,    -33,      0,    -33,    -33,    -33,    -33,    -33,    -33,    -33,    -33,    -33,    -33,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     40,      0,     41,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,    -40,      0,      0,      0,      0,      0,      0,      0,      0,    -40,      0,    -40,    -40,      0,    -40,      0,    -40,      0,    -40,    -40,    -40,    -40,    -40,    -40,    -40,      0,    -40,    -40,    -40,    -40,    -40,    -40,    -40,    -40,    -40,    -40,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     43,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,    -41,      0,      0,      0,      0,      0,      0,      0,      0,    -41,      0,    -41,    -41,      0,    -41,      0,    -41,      0,    -41,    -41,    -41,    -41,    -41,    -41,    -41,      0,    -41,    -41,    -41,    -41,    -41,    -41,    -41,    -41,    -41,    -41,  },
{      0,    -42,      0,      0,      0,      0,      0,      0,      0,      0,    -42,      0,    -42,    -42,      0,    -42,      0,    -42,      0,    -42,    -42,    -42,    -42,    -42,     38,    -42,      0,    -42,    -42,    -42,    -42,    -42,    -42,    -42,    -42,    -42,    -42,  },
{      0,     37,      0,      0,      0,      0,      0,      0,      0,      0,    -35,      0,    -35,    -35,      0,    -35,      0,    -35,      0,    -35,    -35,    -35,    -35,    -35,     38,    -35,      0,    -35,    -35,    -35,    -35,    -35,    -35,    -35,    -35,    -35,    -35,  },
{      0,     37,      0,      0,      0,      0,      0,      0,      0,      0,    -34,      0,    -34,    -34,      0,    -34,      0,    -34,      0,    -34,    -34,    -34,    -34,    -34,     38,    -34,      0,    -34,    -34,    -34,    -34,    -34,    -34,    -34,    -34,    -34,    -34,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     48,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,    -39,      0,      0,      0,      0,      0,      0,      0,      0,    -39,      0,    -39,    -39,      0,    -39,      0,    -39,      0,    -39,    -39,    -39,    -39,    -39,    -39,    -39,      0,    -39,    -39,    -39,    -39,    -39,    -39,    -39,    -39,    -39,    -39,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -15,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -30,      0,      0,      0,      0,    -30,      0,      0,    -30,    -30,      0,      0,     51,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     53,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -31,      0,      0,      0,      0,    -31,      0,      0,    -31,    -31,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,    -37,      0,      0,      0,      0,      0,      0,      0,      0,    -37,      0,    -37,    -37,      0,    -37,      0,    -37,      0,    -37,    -37,    -37,    -37,    -37,     38,    -37,      0,    -37,    -37,    -37,    -37,    -37,    -37,    -37,    -37,    -37,    -37,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     56,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     58,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     60,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,    -28,    -28,    -28,      0,    -28,    -28,    -28,    -28,      0,    -28,      0,      0,    -28,      0,      0,      0,    -28,      0,      0,      0,      0,      0,      0,      0,    -28,    -28,    -28,      0,      0,      0,      0,      0,      0,      0,      0,  },
{    -12,      0,      0,      0,      0,    -12,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     63,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -13,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -14,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     68,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     69,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     71,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,    -38,      0,      0,      0,      0,      0,      0,      0,      0,    -38,      0,    -38,    -38,      0,    -38,      0,    -38,      0,    -38,    -38,    -38,    -38,    -38,    -38,    -38,      0,    -38,    -38,    -38,    -38,    -38,    -38,    -38,    -38,    -38,    -38,  },
{      0,      0,      0,     73,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -20,      0,    -20,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,    -10,    -10,    -10,      0,    -10,    -10,    -10,    -10,      0,    -10,      0,      0,    -10,      0,      0,      0,    -10,      0,      0,      0,      0,      0,      0,      0,    -10,    -10,    -10,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -16,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -17,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -18,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -27,      0,      0,    -27,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     84,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -19,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,    -29,    -29,    -29,      0,    -29,    -29,    -29,    -29,      0,    -29,      0,      0,    -29,      0,      0,      0,    -29,      0,      0,      0,      0,      0,      0,      0,    -29,    -29,    -29,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -23,      0,      0,      0,      0,    -23,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -21,      0,    -21,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     91,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -24,      0,      0,      0,      0,    -24,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -22,      0,    -22,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -25,      0,      0,      0,      0,    -25,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -26,      0,      0,    -26,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,     37,      0,      0,      0,      0,      0,      0,      0,      0,    -44,      0,    -44,    -44,      0,    -44,      0,    -44,      0,    -44,    -44,    -44,    -44,    -44,     38,    -44,      0,    -44,    -44,    -44,    -44,    -44,    -44,    -44,    -44,    -44,    -44,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -48,      0,    -48,    -48,      0,    -48,      0,    -48,      0,    -48,    -48,    -48,    -48,    -48,      0,    -48,      0,    -48,    -48,     98,    -48,    -48,    -48,    -48,    -48,    -48,    -48,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -47,      0,    -47,    -47,      0,    -47,      0,    -47,      0,    -47,    -47,    -47,    -47,    -47,      0,    -47,      0,    -47,    -47,     98,    -47,    -47,    -47,    -47,    -47,    -47,    -47,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -46,      0,    -46,    -46,      0,    -46,      0,    -46,      0,    -46,    -46,    -46,    -46,    -46,      0,    -46,      0,    -46,    -46,     98,    -46,    -46,    -46,    -46,    -46,    -46,    -46,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -52,      0,    -52,    -52,      0,    -52,      0,    -52,      0,    -52,    -52,    -52,    -52,    -52,      0,    -52,      0,    -52,    -52,      0,    100,    101,    102,    -52,    -52,    -52,    -52,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -51,      0,    -51,    -51,      0,    -51,      0,    -51,      0,    -51,    -51,    -51,    -51,    -51,      0,    -51,      0,    -51,    -51,      0,    100,    101,    102,    -51,    -51,    -51,    -51,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -50,      0,    -50,    -50,      0,    -50,      0,    -50,      0,    -50,    -50,    -50,    -50,    -50,      0,    -50,      0,    -50,    -50,      0,    100,    101,    102,    -50,    -50,    -50,    -50,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    116,      0,      0,      0,    112,    113,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     22,     36,     24,      0,      0,      0,      0,      0,      0,     29,      0,      0,      0,      0,      0,      0,     31,      0,      0,      0,      0,      0,      0,      0,     32,     33,     34,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -57,      0,    -57,    -57,      0,    -57,      0,    -57,      0,    -57,    -57,    -57,    -57,    -57,      0,    -57,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -55,      0,    -55,    -55,      0,    -55,      0,    -55,      0,    -55,    -55,    -55,    -55,    -55,      0,    -55,      0,    -55,    -55,      0,      0,      0,      0,    106,    107,    108,    -55,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    -54,      0,    -54,    -54,      0,    -54,      0,    -54,      0,    -54,    -54,    -54,    -54,    -54,      0,    -54,      0,    -54,    -54,      0,      0,      0,      0,    106,    107,    108,    -54,  },
{      0,      0,      0,    123,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -4,      0,      0,      0,      0,     -4,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -2,      0,     -2,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,    128,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    126,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -5,      0,      0,      0,      0,     -5,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -3,      0,     -3,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -6,      0,      0,      0,      0,     -6,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -7,      0,      0,     -7,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,     50,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,     -9,     -9,     -9,      0,     -9,     -9,     -9,     -9,      0,     -9,      0,      0,     -9,      0,      0,      0,     -9,      0,      0,      0,      0,      0,      0,      0,     -9,     -9,     -9,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,     -8,      0,      0,     -8,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,    137,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
{    -11,      0,      0,      0,      0,    -11,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,  },
};

void QdlParser::parse() {
  YYStack  yystack;
  yystack.push(0);

  // Fetch until error (throw) or accept (return)
  while(true) {
    // Current lookahead
    YYSVal          yysval;
    unsigned short  yytok = nextToken(yysval);

    if(yytok <  YYINTERN)  yytok = yyintern[yytok];
    if(yytok >= YYINTERN)  error("Unknown Token");
#ifdef TRACE
    std::cerr << "Read " << yyterms[yytok] << std::endl;
#endif

    // Reduce until shift
    while(true) {
      signed short const  yyact = yyaction[*yystack][yytok];
      if(yyact == 0) {
        std::string                yymsg("Expecting (");
        signed short const *const  yyrow = yyaction[*yystack];
        for(unsigned  i = 0; i < 37; i++) {
          if(yyrow[i])  yymsg.append(yyterms[i]) += '|';
        }
        *yymsg.rbegin() = ')';
        error(yymsg.append(" instead of ").append(yyterms[yytok]));
        return;
      }
      if(yyact >  1) { // shift
#ifdef TRACE
        std::cerr << "Push " << yyterms[yytok] << std::endl;
#endif
        yystack.push(yyact, yysval);
        break;
      }
      else {           // reduce (includes accept)
        YYSVal                yylval;
        unsigned short const  yyrno = (yyact < 0)? -yyact : 0;
        unsigned short const  yylen = yylength[yyrno];
        
#ifdef TRACE
        std::cerr << "Reduce by " << yyrules[yyrno] << std::endl;
#endif
        switch(yyrno) { // do semantic actions
        case 0:         // accept
          return;
case 1: {
#line 371 "QdlParser.ypp"

                    yylval = m_lib.declareComponent(yystack[yylen - 2].name());
                  
#line 824 "QdlParser.cpp"
break;
}
case 2: {
#line 375 "QdlParser.ypp"

                  yystack[yylen - 1].comp().addParameter(yystack[yylen - 3].name());
		  yylval = yystack[yylen - 1];
                
#line 833 "QdlParser.cpp"
break;
}
case 3: {
#line 379 "QdlParser.ypp"

                  yystack[yylen - 1].comp().addParameter(yystack[yylen - 3].name());
		  yylval = yystack[yylen - 1];
                
#line 842 "QdlParser.cpp"
break;
}
case 4: {
#line 384 "QdlParser.ypp"

                  yystack[yylen - 1].comp().addPort(PortDecl::Direction::in, yystack[yylen - 3].name(), yystack[yylen - 3].width());
		  yylval = yystack[yylen - 1];
                
#line 851 "QdlParser.cpp"
break;
}
case 5: {
#line 388 "QdlParser.ypp"

                  yystack[yylen - 1].comp().addPort(PortDecl::Direction::in, yystack[yylen - 4].name(), yystack[yylen - 4].width());
		  yylval = yystack[yylen - 1];
                
#line 860 "QdlParser.cpp"
break;
}
case 6: {
#line 392 "QdlParser.ypp"

 		  yystack[yylen - 1].comp().addPort(PortDecl::Direction::in, yystack[yylen - 3].name(), yystack[yylen - 3].width());
		  yylval = yystack[yylen - 1];
                
#line 869 "QdlParser.cpp"
break;
}
case 7: {
#line 396 "QdlParser.ypp"

                  yystack[yylen - 1].comp().addPort(PortDecl::Direction::out, yystack[yylen - 3].name(), yystack[yylen - 3].width());
		  yylval = yystack[yylen - 1];
                
#line 878 "QdlParser.cpp"
break;
}
case 8: {
#line 400 "QdlParser.ypp"

 		  yystack[yylen - 1].comp().addPort(PortDecl::Direction::out, yystack[yylen - 3].name(), yystack[yylen - 3].width());
		  yylval = yystack[yylen - 1];
		
#line 887 "QdlParser.cpp"
break;
}
case 9: {
#line 404 "QdlParser.ypp"
 yylval = yystack[yylen - 1]; 
#line 893 "QdlParser.cpp"
break;
}
case 10: {
#line 405 "QdlParser.ypp"

		  yystack[yylen - 1].comp().addStatement(yystack[yylen - 2].stmt());
		  yylval = yystack[yylen - 1];
                
#line 902 "QdlParser.cpp"
break;
}
case 13: {
#line 413 "QdlParser.ypp"

            yylval = std::make_shared<ConstDecl>(yystack[yylen - 2].name(), yystack[yylen - 4].expr());
          
#line 910 "QdlParser.cpp"
break;
}
case 14: {
#line 416 "QdlParser.ypp"
 yylval = std::make_shared<ConfigDecl>(yystack[yylen - 2].name(), yystack[yylen - 2].width()); 
#line 916 "QdlParser.cpp"
break;
}
case 15: {
#line 417 "QdlParser.ypp"
 yylval = std::make_shared<SignalDecl>(yystack[yylen - 2].name(), yystack[yylen - 2].width()); 
#line 922 "QdlParser.cpp"
break;
}
case 16: {
#line 418 "QdlParser.ypp"
 yylval = std::make_shared<Equation>(yystack[yylen - 1].expr(), yystack[yylen - 3].expr()); 
#line 928 "QdlParser.cpp"
break;
}
case 17: {
#line 419 "QdlParser.ypp"
 yylval = std::make_shared<InEquation>(yystack[yylen - 1].expr(), yystack[yylen - 3].expr()); 
#line 934 "QdlParser.cpp"
break;
}
case 18: {
#line 420 "QdlParser.ypp"
 yylval = yystack[yylen - 1]; 
#line 940 "QdlParser.cpp"
break;
}
case 19: {
#line 421 "QdlParser.ypp"
 yylval = yystack[yylen - 1]; 
#line 946 "QdlParser.cpp"
break;
}
case 20: {
#line 423 "QdlParser.ypp"

               yylval = std::make_shared<Instantiation>(yystack[yylen - 1].name(), m_lib.resolveComponent(yystack[yylen - 3].name()));
             
#line 954 "QdlParser.cpp"
break;
}
case 21: {
#line 426 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addParameter(yystack[yylen - 3].expr());
	       yylval = yystack[yylen - 1];
             
#line 963 "QdlParser.cpp"
break;
}
case 22: {
#line 430 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addParameter(yystack[yylen - 3].expr());
	       yylval = yystack[yylen - 1];
             
#line 972 "QdlParser.cpp"
break;
}
case 23: {
#line 434 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addConnection(yystack[yylen - 3].expr());
	       yylval = yystack[yylen - 1];
             
#line 981 "QdlParser.cpp"
break;
}
case 24: {
#line 438 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addConnection(yystack[yylen - 4].expr());
	       yylval = yystack[yylen - 1];
             
#line 990 "QdlParser.cpp"
break;
}
case 25: {
#line 442 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addConnection(yystack[yylen - 3].expr());
	       yylval = yystack[yylen - 1];
             
#line 999 "QdlParser.cpp"
break;
}
case 26: {
#line 447 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addConnection(yystack[yylen - 3].expr());
	       yylval = yystack[yylen - 1];
             
#line 1008 "QdlParser.cpp"
break;
}
case 27: {
#line 451 "QdlParser.ypp"

               static_cast<Instantiation&>(*yystack[yylen - 1].stmt()).addConnection(yystack[yylen - 3].expr());
	       yylval = yystack[yylen - 1];
             
#line 1017 "QdlParser.cpp"
break;
}
case 28: {
#line 456 "QdlParser.ypp"

	       yylval = std::make_shared<Generate>(yystack[yylen - 2].name(), yystack[yylen - 4].expr(), yystack[yylen - 6].expr());
	     
#line 1025 "QdlParser.cpp"
break;
}
case 29: {
#line 459 "QdlParser.ypp"

	       static_cast<Generate&>(*yystack[yylen - 1].stmt()).addStatement(yystack[yylen - 2].stmt());
	       yylval = yystack[yylen - 1];
	     
#line 1034 "QdlParser.cpp"
break;
}
case 30: {
#line 465 "QdlParser.ypp"

            yylval.makeBus(yystack[yylen - 1].name(), std::make_shared<ConstExpression>(1));
          
#line 1042 "QdlParser.cpp"
break;
}
case 31: {
#line 468 "QdlParser.ypp"

	    yylval.makeBus(yystack[yylen - 1].name(), yystack[yylen - 3].expr());
	  
#line 1050 "QdlParser.cpp"
break;
}
case 32: {
#line 473 "QdlParser.ypp"

            yylval = std::make_shared<ConstExpression>(yystack[yylen - 1].number());
          
#line 1058 "QdlParser.cpp"
break;
}
case 33: {
#line 476 "QdlParser.ypp"

            yylval = std::make_shared<NameExpression>(yystack[yylen - 1].name());
          
#line 1066 "QdlParser.cpp"
break;
}
case 34: {
#line 479 "QdlParser.ypp"

	    yylval = std::make_shared<UniExpression>(UniExpression::Op::NOT, yystack[yylen - 2].expr());
          
#line 1074 "QdlParser.cpp"
break;
}
case 35: {
#line 482 "QdlParser.ypp"

	    yylval = yystack[yylen - 2];
	  
#line 1082 "QdlParser.cpp"
break;
}
case 36: {
#line 485 "QdlParser.ypp"

	    yylval = std::make_shared<UniExpression>(UniExpression::Op::NEG, yystack[yylen - 2].expr());
          
#line 1090 "QdlParser.cpp"
break;
}
case 37: {
#line 488 "QdlParser.ypp"

	    yylval = std::make_shared<UniExpression>(UniExpression::Op::LD, yystack[yylen - 2].expr());
          
#line 1098 "QdlParser.cpp"
break;
}
case 38: {
#line 491 "QdlParser.ypp"

	    yylval = std::make_shared<ChooseExpression>(yystack[yylen - 3].expr(), yystack[yylen - 6].expr());
	  
#line 1106 "QdlParser.cpp"
break;
}
case 39: {
#line 494 "QdlParser.ypp"

	    yylval = yystack[yylen - 2];
	  
#line 1114 "QdlParser.cpp"
break;
}
case 40: {
#line 497 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::SEL, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1122 "QdlParser.cpp"
break;
}
case 41: {
#line 500 "QdlParser.ypp"

	    yylval = std::make_shared<RangeExpression>(yystack[yylen - 1].expr(), yystack[yylen - 3].expr(), yystack[yylen - 5].expr());
          
#line 1130 "QdlParser.cpp"
break;
}
case 42: {
#line 503 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::POW, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1138 "QdlParser.cpp"
break;
}
case 43: {
#line 507 "QdlParser.ypp"

	    yylval = yystack[yylen - 1];
	  
#line 1146 "QdlParser.cpp"
break;
}
case 44: {
#line 510 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::CAT, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1154 "QdlParser.cpp"
break;
}
case 45: {
#line 514 "QdlParser.ypp"

	    yylval = yystack[yylen - 1];
	  
#line 1162 "QdlParser.cpp"
break;
}
case 46: {
#line 517 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::MUL, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1170 "QdlParser.cpp"
break;
}
case 47: {
#line 520 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::DIV, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1178 "QdlParser.cpp"
break;
}
case 48: {
#line 523 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::MOD, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1186 "QdlParser.cpp"
break;
}
case 49: {
#line 527 "QdlParser.ypp"

	    yylval = yystack[yylen - 1];
	  
#line 1194 "QdlParser.cpp"
break;
}
case 50: {
#line 530 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::AND, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1202 "QdlParser.cpp"
break;
}
case 51: {
#line 533 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::OR, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1210 "QdlParser.cpp"
break;
}
case 52: {
#line 536 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::XOR, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1218 "QdlParser.cpp"
break;
}
case 53: {
#line 540 "QdlParser.ypp"

	    yylval = yystack[yylen - 1];
	  
#line 1226 "QdlParser.cpp"
break;
}
case 54: {
#line 543 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::ADD, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1234 "QdlParser.cpp"
break;
}
case 55: {
#line 546 "QdlParser.ypp"

	    yylval = std::make_shared<BiExpression>(BiExpression::Op::SUB, yystack[yylen - 1].expr(), yystack[yylen - 3].expr());
          
#line 1242 "QdlParser.cpp"
break;
}
case 56: {
#line 549 "QdlParser.ypp"

	    yylval = yystack[yylen - 1];
	  
#line 1250 "QdlParser.cpp"
break;
}
case 57: {
#line 552 "QdlParser.ypp"

	    yylval = std::make_shared<CondExpression>(yystack[yylen - 1].expr(), yystack[yylen - 3].expr(), yystack[yylen - 5].expr());
          
#line 1258 "QdlParser.cpp"
break;
}
        }
        
        yystack.pop(yylen);
        yystack.push(yygoto[*yystack][yylhs[yyrno]], yylval);
      }
    }
  }
}
