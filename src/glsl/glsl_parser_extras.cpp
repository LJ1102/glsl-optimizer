/*
 * Copyright © 2008, 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

extern "C" {
#include <talloc.h>
#include "main/mtypes.h"
}

#include "ast.h"
#include "glsl_parser_extras.h"
#include "glsl_parser.h"

const char *
_mesa_glsl_shader_target_name(enum _mesa_glsl_parser_targets target)
{
   switch (target) {
   case vertex_shader:   return "vertex";
   case fragment_shader: return "fragment";
   case geometry_shader: return "geometry";
   case ir_shader:       break;
   }

   assert(!"Should not get here.");
}


void
_mesa_glsl_error(YYLTYPE *locp, _mesa_glsl_parse_state *state,
		 const char *fmt, ...)
{
   va_list ap;

   state->error = true;

   assert(state->info_log != NULL);
   state->info_log = talloc_asprintf_append(state->info_log,
					    "%u:%u(%u): error: ",
					    locp->source,
					    locp->first_line,
					    locp->first_column);
   va_start(ap, fmt);
   state->info_log = talloc_vasprintf_append(state->info_log, fmt, ap);
   va_end(ap);
   state->info_log = talloc_strdup_append(state->info_log, "\n");
}


void
_mesa_glsl_warning(const YYLTYPE *locp, _mesa_glsl_parse_state *state,
		   const char *fmt, ...)
{
   va_list ap;

   assert(state->info_log != NULL);
   state->info_log = talloc_asprintf_append(state->info_log,
					    "%u:%u(%u): warning: ",
					    locp->source,
					    locp->first_line,
					    locp->first_column);
   va_start(ap, fmt);
   state->info_log = talloc_vasprintf_append(state->info_log, fmt, ap);
   va_end(ap);
   state->info_log = talloc_strdup_append(state->info_log, "\n");
}


bool
_mesa_glsl_process_extension(const char *name, YYLTYPE *name_locp,
			     const char *behavior, YYLTYPE *behavior_locp,
			     _mesa_glsl_parse_state *state)
{
   enum {
      extension_disable,
      extension_enable,
      extension_require,
      extension_warn
   } ext_mode;

   if (strcmp(behavior, "warn") == 0) {
      ext_mode = extension_warn;
   } else if (strcmp(behavior, "require") == 0) {
      ext_mode = extension_require;
   } else if (strcmp(behavior, "enable") == 0) {
      ext_mode = extension_enable;
   } else if (strcmp(behavior, "disable") == 0) {
      ext_mode = extension_disable;
   } else {
      _mesa_glsl_error(behavior_locp, state,
		       "Unknown extension behavior `%s'",
		       behavior);
      return false;
   }

   bool unsupported = false;

   if (strcmp(name, "all") == 0) {
      if ((ext_mode == extension_enable) || (ext_mode == extension_require)) {
	 _mesa_glsl_error(name_locp, state, "Cannot %s all extensions",
			  (ext_mode == extension_enable)
			  ? "enable" : "require");
	 return false;
      }
   } else if (strcmp(name, "GL_ARB_draw_buffers") == 0) {
      /* This extension is only supported in fragment shaders.
       */
      if (state->target != fragment_shader) {
	 unsupported = true;
      } else {
	 state->ARB_draw_buffers_enable = (ext_mode != extension_disable);
	 state->ARB_draw_buffers_warn = (ext_mode == extension_warn);
      }
   } else if (strcmp(name, "GL_ARB_texture_rectangle") == 0) {
      state->ARB_texture_rectangle_enable = (ext_mode != extension_disable);
      state->ARB_texture_rectangle_warn = (ext_mode == extension_warn);
   } else if (strcmp(name, "GL_EXT_texture_array") == 0) {
      state->EXT_texture_array_enable = (ext_mode != extension_disable);
      state->EXT_texture_array_warn = (ext_mode == extension_warn);

      unsupported = !state->extensions->EXT_texture_array;
   } else {
      unsupported = true;
   }

   if (unsupported) {
      static const char *const fmt = "extension `%s' unsupported in %s shader";

      if (ext_mode == extension_require) {
	 _mesa_glsl_error(name_locp, state, fmt,
			  name, _mesa_glsl_shader_target_name(state->target));
	 return false;
      } else {
	 _mesa_glsl_warning(name_locp, state, fmt,
			    name, _mesa_glsl_shader_target_name(state->target));
      }
   }

   return true;
}

void
_mesa_ast_type_qualifier_print(const struct ast_type_qualifier *q)
{
   if (q->constant)
      printf("const ");

   if (q->invariant)
      printf("invariant ");

   if (q->attribute)
      printf("attribute ");

   if (q->varying)
      printf("varying ");

   if (q->in && q->out) 
      printf("inout ");
   else {
      if (q->in)
	 printf("in ");

      if (q->out)
	 printf("out ");
   }

   if (q->centroid)
      printf("centroid ");
   if (q->uniform)
      printf("uniform ");
   if (q->smooth)
      printf("smooth ");
   if (q->flat)
      printf("flat ");
   if (q->noperspective)
      printf("noperspective ");
}


void
ast_node::print(void) const
{
   printf("unhandled node ");
}


ast_node::ast_node(void)
{
   /* empty */
}


static void
ast_opt_array_size_print(bool is_array, const ast_expression *array_size)
{
   if (is_array) {
      printf("[ ");

      if (array_size)
	 array_size->print();

      printf("] ");
   }
}


void
ast_compound_statement::print(void) const
{
   printf("{\n");
   
   foreach_list_const(n, &this->statements) {
      ast_node *ast = exec_node_data(ast_node, n, link);
      ast->print();
   }

   printf("}\n");
}


ast_compound_statement::ast_compound_statement(int new_scope,
					       ast_node *statements)
{
   this->new_scope = new_scope;

   if (statements != NULL) {
      this->statements.push_degenerate_list_at_head(&statements->link);
   }
}


void
ast_expression::print(void) const
{
   switch (oper) {
   case ast_assign:
   case ast_mul_assign:
   case ast_div_assign:
   case ast_mod_assign:
   case ast_add_assign:
   case ast_sub_assign:
   case ast_ls_assign:
   case ast_rs_assign:
   case ast_and_assign:
   case ast_xor_assign:
   case ast_or_assign:
      subexpressions[0]->print();
      printf("%s ", operator_string(oper));
      subexpressions[1]->print();
      break;

   case ast_field_selection:
      subexpressions[0]->print();
      printf(". %s ", primary_expression.identifier);
      break;

   case ast_plus:
   case ast_neg:
   case ast_bit_not:
   case ast_logic_not:
   case ast_pre_inc:
   case ast_pre_dec:
      printf("%s ", operator_string(oper));
      subexpressions[0]->print();
      break;

   case ast_post_inc:
   case ast_post_dec:
      subexpressions[0]->print();
      printf("%s ", operator_string(oper));
      break;

   case ast_conditional:
      subexpressions[0]->print();
      printf("? ");
      subexpressions[1]->print();
      printf(": ");
      subexpressions[1]->print();
      break;

   case ast_array_index:
      subexpressions[0]->print();
      printf("[ ");
      subexpressions[1]->print();
      printf("] ");
      break;

   case ast_function_call: {
      subexpressions[0]->print();
      printf("( ");

      foreach_list_const (n, &this->expressions) {
	 if (n != this->expressions.get_head())
	    printf(", ");

	 ast_node *ast = exec_node_data(ast_node, n, link);
	 ast->print();
      }

      printf(") ");
      break;
   }

   case ast_identifier:
      printf("%s ", primary_expression.identifier);
      break;

   case ast_int_constant:
      printf("%d ", primary_expression.int_constant);
      break;

   case ast_uint_constant:
      printf("%u ", primary_expression.uint_constant);
      break;

   case ast_float_constant:
      printf("%f ", primary_expression.float_constant);
      break;

   case ast_bool_constant:
      printf("%s ",
	     primary_expression.bool_constant
	     ? "true" : "false");
      break;

   case ast_sequence: {
      printf("( ");
      foreach_list_const(n, & this->expressions) {
	 if (n != this->expressions.get_head())
	    printf(", ");

	 ast_node *ast = exec_node_data(ast_node, n, link);
	 ast->print();
      }
      printf(") ");
      break;
   }

   default:
      assert(0);
      break;
   }
}

ast_expression::ast_expression(int oper,
			       ast_expression *ex0,
			       ast_expression *ex1,
			       ast_expression *ex2)
{
   this->oper = ast_operators(oper);
   this->subexpressions[0] = ex0;
   this->subexpressions[1] = ex1;
   this->subexpressions[2] = ex2;
}


void
ast_expression_statement::print(void) const
{
   if (expression)
      expression->print();

   printf("; ");
}


ast_expression_statement::ast_expression_statement(ast_expression *ex) :
   expression(ex)
{
   /* empty */
}


void
ast_function::print(void) const
{
   return_type->print();
   printf(" %s (", identifier);

   foreach_list_const(n, & this->parameters) {
      ast_node *ast = exec_node_data(ast_node, n, link);
      ast->print();
   }

   printf(")");
}


ast_function::ast_function(void)
   : is_definition(false), signature(NULL)
{
   /* empty */
}


void
ast_fully_specified_type::print(void) const
{
   _mesa_ast_type_qualifier_print(& qualifier);
   specifier->print();
}


void
ast_parameter_declarator::print(void) const
{
   type->print();
   if (identifier)
      printf("%s ", identifier);
   ast_opt_array_size_print(is_array, array_size);
}


void
ast_function_definition::print(void) const
{
   prototype->print();
   body->print();
}


void
ast_declaration::print(void) const
{
   printf("%s ", identifier);
   ast_opt_array_size_print(is_array, array_size);

   if (initializer) {
      printf("= ");
      initializer->print();
   }
}


ast_declaration::ast_declaration(char *identifier, int is_array,
				 ast_expression *array_size,
				 ast_expression *initializer)
{
   this->identifier = identifier;
   this->is_array = is_array;
   this->array_size = array_size;
   this->initializer = initializer;
}


void
ast_declarator_list::print(void) const
{
   assert(type || invariant);

   if (type)
      type->print();
   else
      printf("invariant ");

   foreach_list_const (ptr, & this->declarations) {
      if (ptr != this->declarations.get_head())
	 printf(", ");

      ast_node *ast = exec_node_data(ast_node, ptr, link);
      ast->print();
   }

   printf("; ");
}


ast_declarator_list::ast_declarator_list(ast_fully_specified_type *type)
{
   this->type = type;
   this->invariant = false;
}

void
ast_jump_statement::print(void) const
{
   switch (mode) {
   case ast_continue:
      printf("continue; ");
      break;
   case ast_break:
      printf("break; ");
      break;
   case ast_return:
      printf("return ");
      if (opt_return_value)
	 opt_return_value->print();

      printf("; ");
      break;
   case ast_discard:
      printf("discard; ");
      break;
   }
}


ast_jump_statement::ast_jump_statement(int mode, ast_expression *return_value)
{
   this->mode = ast_jump_modes(mode);

   if (mode == ast_return)
      opt_return_value = return_value;
}


void
ast_selection_statement::print(void) const
{
   printf("if ( ");
   condition->print();
   printf(") ");

   then_statement->print();

   if (else_statement) {
      printf("else ");
      else_statement->print();
   }
   
}


ast_selection_statement::ast_selection_statement(ast_expression *condition,
						 ast_node *then_statement,
						 ast_node *else_statement)
{
   this->condition = condition;
   this->then_statement = then_statement;
   this->else_statement = else_statement;
}


void
ast_iteration_statement::print(void) const
{
   switch (mode) {
   case ast_for:
      printf("for( ");
      if (init_statement)
	 init_statement->print();
      printf("; ");

      if (condition)
	 condition->print();
      printf("; ");

      if (rest_expression)
	 rest_expression->print();
      printf(") ");

      body->print();
      break;

   case ast_while:
      printf("while ( ");
      if (condition)
	 condition->print();
      printf(") ");
      body->print();
      break;

   case ast_do_while:
      printf("do ");
      body->print();
      printf("while ( ");
      if (condition)
	 condition->print();
      printf("); ");
      break;
   }
}


ast_iteration_statement::ast_iteration_statement(int mode,
						 ast_node *init,
						 ast_node *condition,
						 ast_expression *rest_expression,
						 ast_node *body)
{
   this->mode = ast_iteration_modes(mode);
   this->init_statement = init;
   this->condition = condition;
   this->rest_expression = rest_expression;
   this->body = body;
}


void
ast_struct_specifier::print(void) const
{
   printf("struct %s { ", name);
   foreach_list_const(n, &this->declarations) {
      ast_node *ast = exec_node_data(ast_node, n, link);
      ast->print();
   }
   printf("} ");
}


ast_struct_specifier::ast_struct_specifier(char *identifier,
					   ast_node *declarator_list)
{
   name = identifier;
   this->declarations.push_degenerate_list_at_head(&declarator_list->link);
}