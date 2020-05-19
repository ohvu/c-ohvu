struct s_symbol_data;
struct s_cons_data;
struct s_builtin_data;
struct s_function_data;
struct s_lambda_data;
struct s_statement_data;

typedef enum s_expr_type {
	ERROR,
	SYMBOL,
	CONS,
	NIL,
	BUILTIN,
	FUNCTION,
	QUOTE,
	LAMBDA,
	STATEMENT,
	CHARACTER,
	STRING,
	INTEGER,
	BIG_INTEGER
} s_expr_type;

typedef struct s_expr {
	s_expr_type type;
	_Atomic(int32_t)* ref_count;
	union {
		UChar* error;
		struct s_symbol_data* symbol;
		struct s_cons_data* cons;
		void* nil;
		struct s_builtin_data* builtin;
		struct s_function_data* function;
		struct s_expr* quote;
		struct s_lambda_data* lambda;
		struct s_statement_data* statement;
		UChar32 character;
		UChar* string;
		int64_t integer;
	};
} s_expr;

typedef struct s_symbol_data {
	UChar* name;
	s_expr* qualifier;
} s_symbol_data;

typedef struct s_cons_data {
	s_expr car;
	s_expr cdr;
} s_cons_data;

typedef struct s_trie {
	s_expr key;
	int64_t offset;
	uint64_t[4] population;
	s_trie[] children;

	/*
	 * This is a trie mapping symbol names & qualifiers to symbols, used
	 * for interning. Keys are composed of a fixed-length pointer and a
	 * variable-length string, which can be viewed as a variable-length
	 * sequence of bytes. Pretty well understood any easy to optimise.
	 *
	 * The fixed-length qualifiers will make for many common prefixes, so
	 * we should optimise for this. Perhaps a two-level data structure
	 * where we map by qualifier first then switch to a different strategy
	 * for subtrees?
	 *
	 * Since symbols are reference counted, they should remove themselves
	 * from this trie once they're free.
	 */

	/*
	 * First attempt should by 256-way prefix-compressed trie with
	 * popcount compression on arrays.
	 *
	 * TODO common prefix length, reshuffle to try find unique position?
	 *
	 * TODO optimise for retrieval of existing keys not insertion!
	 */
	

}

typedef struct s_tree {
	s_expr* key;
	s_expr value;
	int8_t offset;
	int16_t population;
	s_tree[] children;

	/*
	 * Tree from symbols to expression.
	 *
	 * Since symbols are interned, we can distinguish them uniquely by pointer.
	 *
	 * This means we have a unique 32/64 bit key ready to go, so perhaps a clever
	 * trie implementation can have good results and be simple.
	 *
	 * For some tables (e.g. captured bindings) we have a small, fixed set of keys
	 * and want to make many copies of the table with different values. To achieve
	 * this perhaps we can find a perfect hash for a set of keys? Or just make a
	 * best-effort to find an offset into the pointer data which already produces
	 * a unique byte/nibble for the pointer data. In those cases we might also be
	 * able to assume that all lookups will succeed in which case can omit key.
	 *
	 * Can also try popcount compression on resulting array.
	 */
	;
}

/*
 * An expression can be promoted to a lambda when it
 * appears as a term in a statement. This promotion
 * does not modify the data, it is merely a
 * specialization for the purpose of performance.
 */
typedef struct s_lambda_data {
	int32_t param_count;
	s_expr* params;
	s_expr body;
} s_lambda_data;

typedef struct s_statement_data {
	int32_t free_var_count;
	s_expr* free_vars;
	s_expr target;
	int32_t arg_count;
	s_expr* args;
} s_statement_data;

typedef struct s_function_data {
	s_tree capture;
	s_expr lambda;
} s_function_data;

typedef struct s_builtin_data {
	UChar* name;
	int32_t arg_count;
	bool (*apply)(s_tree* scope, s_expr* result, s_expr* args, void* d);
	void (*free) (void* data);
	void* data;
} s_builtin_data;

s_expr s_nil();
s_expr s_symbol(strref ns, strref n);
s_expr s_cons(s_expr car, s_expr cdr);
s_expr s_character(UChar32 c);
s_expr s_string(strref s);
s_expr s_builtin(strref n, int32_t c,
		bool (*a)(s_tree* scope, s_expr* result, s_expr* a, void* d),
		void (*f)(void* d),
		void* data);
s_expr s_quote(s_expr data);
s_expr s_lambda(int32_t param_count, s_expr* params,
		s_expr body);
s_expr s_statement(int32_t free_var_count, s_expr* free_vars,
		s_expr target,
		int32_t arg_count, s_expr* args);

s_expr s_function(s_tree capture, s_expr lambda);
s_expr s_error(strref message);

UChar* s_name(s_expr s);
UChar* s_namespace(s_expr s);

s_expr s_car(s_expr s);
s_expr s_cdr(s_expr s);
bool s_atom(s_expr e);
bool s_eq(s_expr a, s_expr b);

void s_ref(s_expr s);
void s_free(s_expr s);
void s_dump(s_expr s);
