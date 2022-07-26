#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double real;

typedef struct Tok {
	enum {
		TokNull,
		TokOp,
		TokNum,
		TokIdent,
	} kind;

	union {
		real Num;
		char Char;
		char *Str;
	};
} Tok;

#define TOKS_CAP 65536
static Tok toks[TOKS_CAP];
static size_t toks_size = 0;

static uint8_t op_prec[256] = {
	['('] = 0, /* A precedence of 0 is reserved for delimiters. */
	[')'] = 0,
	[','] = 0,
	['+'] = 1,
	['-'] = 1,
	['*'] = 2,
	['/'] = 2,
	['^'] = 3,
};
#define OP_PREC(tok_char) (op_prec[(size_t)tok_char])

static enum {
	OrderLtr,
	OrderRtl,
} op_order[256] = {
	['('] = OrderLtr,
	[')'] = OrderLtr,
	['+'] = OrderLtr,
	['-'] = OrderLtr,
	['*'] = OrderLtr,
	['/'] = OrderLtr,
	['^'] = OrderRtl,
};
#define OP_ORDER(tok_char) (op_order[(size_t)tok_char])

#define IS_FLOAT(c) ((c >= '0' && c <= '9') || c == '.')
#define IS_ALPHA(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))

typedef struct Var {
	const char *name;
	real val;
} Var;

#define VARS_CAP 256
static Var vars[VARS_CAP];
static size_t vars_size = 0;

static void set_var(const char *name, real val) {
	for (size_t i = 0; i < vars_size; i++) {
		if (strcmp(vars[i].name, name) == 0) {
			vars[i].val = val;
			return;
		}
	}
	vars[vars_size++] = (Var){.name = name, .val = val};
}

static void unset_var(const char *name) {
	for (size_t i = 0; i < vars_size; i++) {
		if (strcmp(vars[i].name, name) == 0) {
			memmove(vars + i, vars + i + 1, sizeof(Var) * (vars_size - (i + 1)));
			vars_size--;
			return;
		}
	}
}

typedef struct Function {
	const char *name;
	real (*func)(real *args);
	size_t n_args;
} Function;

#define FUNCTIONS_CAP 256
static Function functions[FUNCTIONS_CAP];
static size_t functions_size = 0;

static void push_tok(Tok t) {
	if (toks_size < TOKS_CAP)
		toks[toks_size++] = t;
}

static void add_func(const char *name, real (*func)(real *args), size_t n_args) {
	if (functions_size < FUNCTIONS_CAP)
		functions[functions_size++] = (Function){.name = name, .func = func, .n_args = n_args};
}

static void tokenize(char *expr) {
	push_tok((Tok){.kind = TokOp, .Char = '('});

	size_t paren_depth = 0;

	Tok last;

	char *curr = expr;
	for (char c = *curr; c != 0; c = *(++curr)) {
		if (toks_size > 0)
			last = toks[toks_size-1];
		else
			last = (Tok){.kind = TokNull};

		if (c == ' ')
			continue;

		if (IS_FLOAT(c)) {
			char buf[16];
			buf[0] = c;
			size_t i = 1;
			while (i < 15 && IS_FLOAT(curr[i])) {
				buf[i] = curr[i];
				i++;
			}
			curr += i - 1;
			buf[i++] = 0;

			real num = strtod(buf, NULL);

			if (last.kind == TokIdent || (last.kind == TokOp && last.Char == ')') || last.kind == TokNum)
				push_tok((Tok){.kind = TokOp, .Char = '*'});

			push_tok((Tok){.kind = TokNum, .Num = num});
			continue;
		}

		if (IS_ALPHA(c)) {
			char *buf = malloc(32);
			buf[0] = c;
			size_t i = 1;
			while (i < 31 && IS_ALPHA(curr[i])) {
				buf[i] = curr[i];
				i++;
			}
			curr += i - 1;
			buf[i++] = 0;

			if (last.kind == TokIdent || (last.kind == TokOp && last.Char == ')') || last.kind == TokNum)
				push_tok((Tok){.kind = TokOp, .Char = '*'});

			push_tok((Tok){.kind = TokIdent, .Str = buf});
			continue;
		}

		if (c == '(')
			paren_depth++;
		else if (c == ')') {
			if (paren_depth == 0) {
				fprintf(stderr, "Error: unmatched ')'\n");
				exit(1);
			}
			paren_depth--;
		}

		switch (c) {
		case '(':
		case ')':
		case ',':
		case '+':
		case '-':
		case '*':
		case '/':
		case '^': {
			if (c == '(' && ((last.kind == TokOp && last.Char == ')') || last.kind == TokNum))
				push_tok((Tok){.kind = TokOp, .Char = '*'});
			push_tok((Tok){.kind = TokOp, .Char = c});
			break;
		}
		default:
			fprintf(stderr, "Error: unrecognized token at %zd: '%c'\n", curr - expr, c);
			exit(1);
		}
	}

	if (paren_depth > 0) {
		fprintf(stderr, "Error: unmatched '('\n");
		exit(1);
	}

	push_tok((Tok){.kind = TokOp, .Char = ')'});
}

static void print_toks() {
	for (size_t i = 0; i < toks_size; i++) {
		switch (toks[i].kind) {
		case TokOp:
			if (toks[i].Char == 0) {
				fprintf(stderr, "Error: unexpected end of token stream\n");
				exit(1);
			}
			printf("%c ", toks[i].Char);
			break;
		case TokNum:
			printf("%.2f ", toks[i].Num);
			break;
		case TokIdent:
			printf("%s ", toks[i].Str);
			break;
		default:
			fprintf(stderr, "Error: unhandled token\n");
			exit(1);
		}
	}
	printf("\n");
}

/* Delete tokens from begin to end (excluding end itself). */
static void del_toks(Tok *begin, Tok *end) {
	memmove(begin, end, (toks_size - (end - toks)) * sizeof(Tok));
	toks_size -= end - begin;
}

static real eval(Tok *t);

static void collapse(Tok *t) {
	/* Collapse factor. */
	if (t[1].kind == TokOp && t[1].Char == '-') {
		collapse(t + 1);
		if (t[2].kind != TokNum) {
			fprintf(stderr, "Error: uncollapsable expression after minus factor\n");
			exit(1);
		}
		t[2].Num *= -1.0;
		del_toks(t + 1, t + 2);
	}

	/* Collapse parentheses. */
	if (t[1].kind == TokOp && t[1].Char == '(') {
		real res = eval(t + 1);
		size_t i;
		for (i = 2; !(t[i].kind == TokOp && OP_PREC(t[i].Char) == 0); i++);
		del_toks(t + 2, t + i + 1);
		/* Put the newly evaluated value into place. */
		t[1].kind = TokNum;
		t[1].Num = res;
	}


	if (t[1].kind == TokIdent) {
		if (t + 2 < toks + toks_size && (t[2].kind == TokOp && t[2].Char == '(')) {
			/* Collapse function. */
			real arg_results[16];
			size_t arg_results_size = 0;

			t += 2;
			while (1) {
				if (arg_results_size < 16)
					arg_results[arg_results_size++] = eval(t);
				size_t i = 1;
				for (; !(t[i].kind == TokOp && OP_PREC(t[i].Char) == 0); i++);
				bool end = t[i].Char == ')';
				if (t[i].Char == ',')
					del_toks(t, t + i);
				else if (t[i].Char == ')')
					del_toks(t, t + i + 1);
				if (end)
					break;
			}
			t -= 2;

			real outer_res;
			bool func_found = false;
			for (size_t i = 0; i < functions_size; i++) {
				if (strcmp(t[1].Str, functions[i].name) == 0) {
					func_found = true;
					if (arg_results_size != functions[i].n_args) {
						const char *plural = functions[i].n_args == 1 ? "" : "s";
						fprintf(stderr, "Error: function %s() requires exactly 1 argument%s\n", functions[i].name, plural);
						exit(1);
					}
					outer_res = functions[i].func(arg_results);
				}
			}
			if (!func_found) {
				fprintf(stderr, "Error: unknown function: %s()\n", t[1].Str);
				exit(1);
			}

			t[1].kind = TokNum;
			t[1].Num = outer_res;
		} else {
			/* Collapse variable. */
			real res;
			bool found = false;
			for (size_t i = 0; i < vars_size; i++) {
				if (strcmp(t[1].Str, vars[i].name) == 0) {
					found = true;
					res = vars[i].val;
				}
			}
			if (!found) {
				fprintf(stderr, "Error: unknown variable: %s\n", t[1].Str);
				exit(1);
			}
			t[1].kind = TokNum;
			t[1].Num = res;
		}
	}
}

static real eval(Tok *t) {
	if (!(t[0].kind == TokOp && OP_PREC(t[0].Char) == 0)) {
		fprintf(stderr, "Error: expected delimiter at beginning of expression\n");
		exit(1);
	}

	while (1) {
		collapse(t);

		if (!(t[0].kind == TokOp && t[1].kind == TokNum && t[2].kind == TokOp)) {
			fprintf(stderr, "Error: invalid token order\n");
			exit(1);
		}

		const char curr_op = t[0].Char;
		const uint8_t curr_prec = OP_PREC(curr_op);

		const char next_op = t[2].Char;
		const uint8_t next_prec = OP_PREC(next_op);

		/* Delimiters have a precedence of 0; if we have a number between two delimiters, we're done. */
		if (curr_prec == 0 && next_prec == 0)
			return t[1].Num;

		if (next_prec > curr_prec || (next_prec == curr_prec && OP_ORDER(curr_op) == OrderRtl)) {
			t += 2;
		} else if (next_prec < curr_prec || (next_prec == curr_prec && OP_ORDER(curr_op) == OrderLtr)) {
			real res;
			real lhs = t[-1].Num, rhs = t[1].Num;
			switch (curr_op) {
			case '+': res = lhs + rhs; break;
			case '-': res = lhs - rhs; break;
			case '*': res = lhs * rhs; break;
			case '/': res = lhs / rhs; break;
			case '^': res = pow(lhs, rhs); break;
			default:
				fprintf(stderr, "Error: unhandled operator '%c'\n", t[1].Char);
				exit(1);
			}

			t[1].Num = res;

			del_toks(t - 1, t + 1);

			t -= 2;
		}
	}
}

static void cleanup() {
	for (size_t i = 0; i < toks_size; i++) {
		if (toks[i].kind == TokIdent)
			free(toks[i].Str);
	}
}

static real fn_sqrt(real *args)  { return sqrt(args[0]); }
static real fn_pow(real *args)   { return pow(args[0], args[1]); }
static real fn_mod(real *args)   { return fmod(args[0], args[1]); }
static real fn_round(real *args) { return round(args[0]); }
static real fn_floor(real *args) { return floor(args[0]); }
static real fn_ceil(real *args)  { return ceil(args[0]); }
static real fn_sin(real *args) { return sin(args[0]); }
static real fn_cos(real *args)  { return cos(args[0]); }

int main(int argc, char **argv) {
	if (argc != 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		fprintf(stderr, "Usage: ./exp \"<expression>\"\n");
		exit(1);
	}
	add_func("sqrt", fn_sqrt, 1);
	add_func("pow", fn_pow, 2);
	add_func("mod", fn_mod, 2);
	add_func("round", fn_round, 1);
	add_func("floor", fn_floor, 1);
	add_func("ceil", fn_ceil, 1);
	add_func("sin", fn_sin, 1);
	add_func("cos", fn_cos, 1);
	unset_var("x");
	set_var("pi", M_PI);
	set_var("e", M_E);
	tokenize(argv[1]);
	print_toks();
	real res = eval(toks);
	printf("Result: %f\n", res);
	cleanup();
}
