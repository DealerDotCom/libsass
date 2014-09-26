#ifndef SASS_UTIL 
#define SASS_UTIL

#ifndef SASS_AST
#include "ast.hpp"
#endif

#include <string>
namespace Sass {
  namespace Util {

    std::string normalize_underscores(const std::string& str);

    bool containsAnyPrintableStatements(Block* b);

		void bubbleMediaQueries(Block*& b, Context& ctx);

  }
}
#endif
