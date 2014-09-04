#include "output_nested.hpp"
#include "inspect.hpp"
#include "ast.hpp"
#include "context.hpp"
#include "to_string.hpp"
#include <iostream>
#include <sstream>
#include <typeinfo>

namespace Sass {
  using namespace std;

  Output_Nested::Output_Nested(bool source_comments, Context* ctx)
  : buffer(""), rendered_imports(""), indentation(0), source_comments(source_comments), ctx(ctx)
  { }
  Output_Nested::~Output_Nested() { }

  inline void Output_Nested::fallback_impl(AST_Node* n)
  {
    Inspect i(ctx);
    n->perform(&i);
    buffer += i.get_buffer();
  }

  void Output_Nested::operator()(Import* imp)
  {
    Inspect insp(ctx);
    imp->perform(&insp);
    if (!rendered_imports.empty()) {
      rendered_imports += "\n";
    }
    rendered_imports += insp.get_buffer();
  }

  static bool hasPrintableChildren(Block* b);
  static bool hasPrintableChildren(Statement* s);

  void Output_Nested::operator()(Block* b)
  {
    if (!b->is_root()) return;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      size_t old_len = buffer.length();
      (*b)[i]->perform(this);
      if (i < L-1 && old_len < buffer.length()) append_to_buffer("\n");
    }
  }
  
  void Output_Nested::operator()(Ruleset* r)
  {
    Selector* s     = r->selector();
    Block*    b     = r->block();
    bool      decls = false;

    Selector_List* sl = static_cast<Selector_List*>(s);

    if (sl->length() == 0) return;
    
    // don't print rulesets that don't have any printable children
    if (!hasPrintableChildren(r)) return;

    if (b->has_non_hoistable()) {
      decls = true;
      indent();
      if (source_comments) {
        stringstream ss;
        ss << "/* line " << r->position().line << ", " << r->path() << " */" << endl;
        append_to_buffer(ss.str());
        indent();
      }
      s->perform(this);
      append_to_buffer(" {\n");
      ++indentation;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement* stm = (*b)[i];
        bool bPrintExpression = true;
        // Check print conditions
        if (typeid(*stm) == typeid(Declaration)) {
          Declaration* dec = static_cast<Declaration*>(stm);
          if (dec->value()->concrete_type() == Expression::STRING) {
            String_Constant* valConst = static_cast<String_Constant*>(dec->value());
            string val(valConst->value());
            if (val.empty()) {
              bPrintExpression = false;
            }
          }
          else if (dec->value()->concrete_type() == Expression::LIST) {
            List* list = static_cast<List*>(dec->value());
            bool all_invisible = true;
            for (size_t list_i = 0, list_L = list->length(); list_i < list_L; ++list_i) {
              Expression* item = (*list)[list_i];
              if (!item->is_invisible()) all_invisible = false;
            }
            if (all_invisible) bPrintExpression = false;
          }
        }
        
        // don't print @extends
        if (typeid(*stm) == typeid(Extension)) {
          bPrintExpression = false;
        }
        
        // Print if OK
        if (!stm->is_hoistable() && bPrintExpression) {
          if (!stm->block()) indent();
          stm->perform(this);
          append_to_buffer("\n");
        }
      }
      --indentation;
      buffer.erase(buffer.length()-1);
      if (ctx) ctx->source_map.remove_line();
      append_to_buffer(" }\n");
    }

    if (b->has_hoistable()) {
      if (decls) ++indentation;
      // indent();
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement* stm = (*b)[i];
        if (stm->is_hoistable()) {
          stm->perform(this);
        }
      }
      if (decls) --indentation;
    }
  }

  void Output_Nested::operator()(Media_Block* m)
  {
    List*  q     = m->media_queries();
    Block* b     = m->block();
    bool   decls = false;

    indent();
    ctx->source_map.add_mapping(m);
    append_to_buffer("@media ");
    q->perform(this);
    append_to_buffer(" {\n");

    Selector* e = m->selector();
    bool hoisted = false;
    if (e && b->has_non_hoistable()) {
      hoisted = true;
      ++indentation;
      indent();
      e->perform(this);
      append_to_buffer(" {\n");
    }

    ++indentation;
    decls = true;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement* stm = (*b)[i];
      if (!stm->is_hoistable()) {
        if (!stm->block()) indent();
        stm->perform(this);
        append_to_buffer("\n");
      }
    }
    --indentation;

    if (hoisted) {
      buffer.erase(buffer.length()-1);
      if (ctx) ctx->source_map.remove_line();
      append_to_buffer(" }\n");
      --indentation;
    }

    if (decls) ++indentation;
    if (hoisted) ++indentation;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement* stm = (*b)[i];
      if (stm->is_hoistable()) {
        stm->perform(this);
      }
    }
    if (hoisted) --indentation;
    if (decls) --indentation;

    buffer.erase(buffer.length()-1);
    if (ctx) ctx->source_map.remove_line();
    append_to_buffer(" }\n");
  }

  void Output_Nested::operator()(At_Rule* a)
  {
    string      kwd   = a->keyword();
    Selector*   s     = a->selector();
    Expression* v     = a->value();
    Block*      b     = a->block();
    bool        decls = false;

    // indent();
    append_to_buffer(kwd);
    if (s) {
      append_to_buffer(" ");
      s->perform(this);
    }
    else if (v) {
      append_to_buffer(" ");
      v->perform(this);
    }

    if (!b) {
      append_to_buffer(";");
      return;
    }

    append_to_buffer(" {\n");
    ++indentation;
    decls = true;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement* stm = (*b)[i];
      if (!stm->is_hoistable()) {
        if (!stm->block()) indent();
        stm->perform(this);
        append_to_buffer("\n");
      }
    }
    --indentation;

    if (decls) ++indentation;
    for (size_t i = 0, L = b->length(); i < L; ++i) {
      Statement* stm = (*b)[i];
      if (stm->is_hoistable()) {
        stm->perform(this);
        append_to_buffer("\n");
      }
    }
    if (decls) --indentation;

    buffer.erase(buffer.length()-1);
    if (ctx) ctx->source_map.remove_line();
    if (b->has_hoistable()) {
      buffer.erase(buffer.length()-1);
      if (ctx) ctx->source_map.remove_line();
    }
    append_to_buffer(" }\n");
  }

  void Output_Nested::indent()
  { append_to_buffer(string(2*indentation, ' ')); }

  void Output_Nested::append_to_buffer(const string& text)
  {
    buffer += text;
    if (ctx) ctx->source_map.update_column(text);
  }

  static bool hasPrintableChildren(Statement* s) {
    if (typeid((*s)) == typeid(Ruleset)) {
      Ruleset* ruleset = (Ruleset*)s;
      Block* block = ruleset->block();
      return hasPrintableChildren(block);
    }
    return true;
  }
  
  static bool hasPrintableChildren(Block* b) {
    if (b != NULL) {
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        if (typeid(*((*b)[i])) == typeid(Block)) {
          return hasPrintableChildren((*b)[i]);
        }
        else if (typeid(*((*b)[i])) != typeid(Extension)) {
          return true;
        }
      }
    }
    return false;
  }

}
