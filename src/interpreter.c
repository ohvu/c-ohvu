#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <unicode/utypes.h>
#include <unicode/ucnv.h>

#include "c-calipto/stringref.h"
#include "c-calipto/idtrie.h"
#include "c-calipto/sexpr.h"
#include "c-calipto/interpreter.h"

typedef struct compile_context {
	idtrie indices;
	s_expr data_quote;
	s_expr data_lambda;
} compile_context;

bool compile_quote(s_term* result, int32_t part_count, s_expr* parts, compile_context c) {
	if (part_count != 1) {
		return false;
	}

	*result = (s_term){ .quote=parts[0] };

	return true;
}

bool compile_statement(s_statement* result, s_expr s, compile_context c);

s_term s_alias_term(s_term t) {
	switch (t.type) {
		case LAMBDA:
			s_ref_lambda(t.lambda);
			break;
		case VARIABLE:
			break;
		default:
			s_alias(t.quote);
			break;
	}
	return t;
}

void s_dealias_term(s_term t) {
	switch (t.type) {
		case LAMBDA:
			s_free_lambda(t.lambda);
			break;
		case VARIABLE:
			break;
		default:
			s_dealias(t.quote);
			break;
	}
}

void* get_ref(s_expr e) {
	return e.p;
}

bool compile_lambda(s_term* result, int32_t part_count, s_expr* parts, compile_context c) {
	if (part_count != 2) {
		return false;
	}

	s_expr params_decl = parts[0];
	s_expr body_decl = parts[1];

	s_expr_ref** params;
	int32_t param_count = s_delist_of(params_decl, (void***)&params, get_ref);
	if (param_count < 0) {
		return false;
	}

	s_statement body;
	bool success = compile_statement(&body, body_decl, c);

	uint32_t var_count = 0;
	uint32_t* vars = NULL;

	if (success) {
		s_lambda* l = malloc(sizeof(s_lambda));
		l->ref_count = ATOMIC_VAR_INIT(1);
		l->param_count = param_count;
		if (param_count > 0) {
			l->params = malloc(sizeof(s_expr_ref*) * param_count);
			for (int i = 0; i < param_count; i++) {
				s_ref(params[i]);
				l->params[i] = params[i];
			}
		} else {
			l->params = NULL;
		}
		l->var_count = var_count;
		if (var_count > 0) {
			l->vars = malloc(sizeof(uint32_t) * var_count);
			for (int i = 0; i < var_count; i++) {
				l->vars[i] = vars[i];
			}
		} else {
			l->vars = NULL;
		}
		l->body = body;
		*result = (s_term){ .type=LAMBDA, .lambda=l };
	}

	return success;
}

s_lambda* s_ref_lambda(s_lambda* l) {
	atomic_fetch_add(&l->ref_count, 1);
}

void s_free_lambda(s_lambda* l) {
	if (atomic_fetch_add(&l->ref_count, -1) > 1) {
		if (l->param_count > 0) {
			for (int i = 0; i < l->param_count; i++) {
				s_free(SYMBOL, l->params[i]);
			}
			free(l->params);
		}
		if (l->var_count > 0) {
			free(l->vars);
		}
		if (l->body.term_count > 0) {
			for (int i = 0; i < l->body.term_count; i++) {
				s_dealias_term(l->body.terms[i]);
			}
			free(l->body.terms);
		}
	}
}

bool compile_expression(s_term* result, s_expr e, compile_context c) {
	if (s_atom(e)) {
		uint32_t* index_into_parent = idtrie_fetch(c.indices, sizeof(s_expr_ref*), e.p);
		*result = (s_term){ .type=VARIABLE, .variable=*index_into_parent };
		return true;
	}

	s_expr* parts;
	uint32_t count = s_delist(e, &parts);
	if (count <= 0) {
		printf("Syntax error in expression: ");
		s_dump(e);
		return false;
	}

	s_expr kind = parts[0];
	int32_t term_count = count - 1;
	s_expr* terms = parts + 1;

	bool success;
	if (s_eq(kind, c.data_quote)) {
		success = compile_quote(result, term_count, terms, c);

	} else if (s_eq(kind, c.data_lambda)) {
		success = compile_lambda(result, term_count, terms, c);

	} else {
		success = false;
	}

	for (int i = 0; i < count; i++) {
		s_dealias(parts[i]);
	}
	free(parts);
	
	return success;
}

bool compile_statement(s_statement* result, s_expr s, compile_context c) {
	s_expr* expressions;
	uint32_t count = s_delist(s, &expressions);
	if (count <= 0) {
		printf("Syntax error in statement: ");
		s_dump(s);
		return false;
	}

	s_term* terms = malloc(sizeof(s_term) * count);
	bool success = true;
	for (int i = 0; i < count; i++) {
		if (compile_expression(&terms[i], expressions[i], c)) {
			s_dealias(expressions[i]);
		} else {
			success = false;
			break;
		}
	}
	free(expressions);

	if (success) {
		*result = (s_statement){ count, terms };
	}

	return success;
}

typedef struct symbol_bindings {
	uint32_t count;
	uint32_t* indices_into_parent;
	idtrie indices;
} symbol_bindings;

typedef struct symbol_index {
	const s_expr_ref* symbol;
	uint32_t index;
} symbol_index;

void* get_symbol_index(void* key, idtrie_node* owner) {
	uint32_t* value = malloc(sizeof(uint32_t*));
	*value = ((symbol_index*)key)->index;
	return value;
}

void update_symbol_index(void* value, idtrie_node* owner) {}

void free_symbol_index(void* value) {
	free(value);
}

void bind_symbol_index(idtrie p, symbol_bindings* b, s_expr_ref* symbol, uint32_t index) {
	symbol_index si = { symbol, index };
	if (b->count == *(uint32_t*)idtrie_insert(b->indices, sizeof(s_expr_ref*), &si)) {
		b->count++;

		uint32_t* old_indices = b->indices_into_parent;
		b->indices_into_parent = malloc(sizeof(uint32_t*) * b->count);
		if (old_indices != NULL) {
			memcpy(b->indices_into_parent, old_indices, sizeof(uint32_t*) * (b->count - 1));
			free(old_indices);
		}

		uint32_t* index_into_parent = idtrie_fetch(p, sizeof(s_expr_ref*), symbol);
		if (index_into_parent == NULL) {
			// TODO ERROR
		}
		b->indices_into_parent[b->count - 1] = *index_into_parent;
	}
}

s_result s_compile(s_statement* result, const s_expr e, const uint32_t param_count, const s_expr_ref** params) {
	compile_context c;
	c.indices.get_value = get_symbol_index;
	c.indices.update_value = update_symbol_index;
	c.indices.free_value = free_symbol_index;
	for (int i = 0; i < param_count; i++) {
		symbol_index si = { params[i], i };
		idtrie_insert(c.indices, sizeof(s_expr_ref*), &si);
	}

	s_expr data = s_symbol(NULL, u_strref(u"data"));
	c.data_quote = s_symbol(data.p, u_strref(u"quote"));
	c.data_lambda = s_symbol(data.p, u_strref(u"lambda"));
	s_dealias(data);
	compile_statement(result, e, c);
	s_dealias(c.data_quote);
	s_dealias(c.data_lambda);

	return S_SUCCESS;
}

#define ARGS_ON_STACK 16
#define ARGS_ON_HEAP 32

typedef struct instruction_slot {
	s_expr stack_values[ARGS_ON_STACK];
	uint32_t heap_values_size;
	s_expr* heap_values;
	uint32_t value_count;
	s_expr* values;
	struct instruction_slot* next;
} instruction_slot;

void prepare_instruction_slot(instruction_slot* i, uint32_t size) {
	i->value_count = size;
	if (size <= ARGS_ON_STACK) {
		i->values = i->stack_values;
	} else {
		if (size > i->heap_values_size) {
			if (i->heap_values_size > 0) {
				free(i->heap_values);
			}
			i->heap_values_size = size;
			i->heap_values = malloc(sizeof(s_expr) * size);
		}
		i->values = i->heap_values;
	}
}

static s_function_type lambda_function = {
	u"lambda",
	represent_lambda,
	arg_count_lambda,
	max_result_size_lambda,
	apply_lambda,
	free_lambda
};

void eval_expression(s_expr* result, s_term e, s_expr* closure) {
	switch (e.type) {
		case VARIABLE:
			*result = closure[e.variable];

		case LAMBDA:
			;
			s_lambda l = {
			};
			*result = s_function(&lambda_function, sizeof(s_lambda), &l);

		default:
			*result = s_alias(e.quote);
	}
}

void eval_statement(instruction_slot* result, s_statement s, s_expr* closure) {
	prepare_instruction_slot(result, s.term_count);

	for (int i = 0; i < s.term_count; i++) {
		eval_expression(&result->values[i], s.terms[i], closure);
	}
}

s_result execute_instruction(instruction_slot* next, instruction_slot* current) {
	bool success;
	if (target.type == FUNCTION) {
		s_function_data* f = &current->values[0].p->function;

		uint32_t max_result_size = f->type->max_result_size(f + 1);
		prepare_instruction_slot(next, max_result_size);

		uint32_t arg_count = f->type->arg_count(f + 1);
		if (arg_count != instruction_size - 1) {
			return S_ARGUMENT_COUNT_MISMATCH;
		}

		success = target.builtin->apply(result, args, target.builtin->data);

	} else {
		printf("Unable to apply to target: ");
		s_dump(target);
		success = false;
		break;
	}

	s_free(target);
	for (int i = 0; i < arg_count; i++) {
		s_free(args[i]);
	}
	free(args);

	return success;
}

s_result s_eval(const s_statement s, const s_expr* args) {
	/*
	 * We have two instruction slots, the current instruction which is
	 * executing, and the next instruction which is being written. We
	 * flip-flop between them, like double buffering.
	 */

	instruction_slot a;
	instruction_slot b;
	a.heap_values_size = 0;
	b.heap_values_size = 0;
	a.next = &b;
	b.next = &a;

	instruction_slot* current = &a;
	s_result r = eval_statement(current, s);
	while (r == S_SUCCESS && current->value_count > 0) {
		if (current->values[0].type == FUNCTION) {
			r = execute_instruction(current->next, current);
		} else {
			r = S_ATTEMPT_TO_CALL_NON_FUNCTION;
		}
		current = current->next;
	}

	if (a.heap_values_size > 0) {
		free(a.heap_values);
	}
	if (b.heap_values_size > 0) {
		free(b.heap_values);
	}

	return r;
}

