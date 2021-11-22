#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double real;

typedef struct Tok {
	enum {
		TokOp,
		TokNum,
		TokFunc,
	} kind;

	union {
		real Num;
		char Char;
		char *Str;
	};
} Tok;

#define TOKS_CAP 65536
Tok toks[TOKS_CAP];
size_t toks_size = 0;

uint8_t op_prec[256] = {
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

enum {
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

void push_tok(Tok t) {
	if (toks_size+1 < TOKS_CAP)
		toks[toks_size++] = t;
}

void tokenize(char *expr) {
	push_tok((Tok){.kind = TokOp, .Char = '('});

	size_t paren_depth = 0;

	char *curr = expr;
	for (char c = *curr; c != 0; c = *(++curr)) {
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

			push_tok((Tok){.kind = TokFunc, .Str = buf});
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

void print_toks() {
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
		case TokFunc:
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
void del_toks(Tok *begin, Tok *end) {
	memmove(begin, end, (toks_size - (end - toks)) * sizeof(Tok));
	toks_size -= end - begin;
}

real eval(Tok *t) {
	if (!(t[0].kind == TokOp && OP_PREC(t[0].Char) == 0)) {
		fprintf(stderr, "Error: expected delimiter at beginning of expression\n");
		exit(1);
	}

	while (1) {
		/* Collapse factor. */
		if (t[1].kind == TokOp && t[1].Char == '-') {
			if (t[2].kind != TokNum) {
				fprintf(stderr, "Error: expected number token after minus factor\n");
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


		/* Collapse function. */
		if (t[1].kind == TokFunc) {
			if (t + 2 >= toks + toks_size || !(t[2].kind == TokOp && t[2].Char == '(')) {
				fprintf(stderr, "Error: expected '(' token after function\n");
				exit(1);
			}

			real arg_results[16];
			size_t arg_results_size = 0;

			t += 2;
			while (1) {
				arg_results[arg_results_size++] = eval(t); /* TODO: Overflow protection. */
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
			if (strcmp(t[1].Str, "sqrt") == 0) {
				if (arg_results_size != 1) {
					fprintf(stderr, "Error: function sqrt() requires exactly 1 argument\n");
					exit(1);
				}
				outer_res = sqrt(arg_results[0]);
			} else if (strcmp(t[1].Str, "pow") == 0) {
				if (arg_results_size != 2) {
					fprintf(stderr, "Error: function pow() requires exactly 2 arguments\n");
					exit(1);
				}
				outer_res = pow(arg_results[0], arg_results[1]);
			} else {
				fprintf(stderr, "Error: unknown function name: %s\n", t[1].Str);
				exit(1);
			}
			t[1].kind = TokNum;
			t[1].Num = outer_res;
		}
		
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

void cleanup() {
	for (size_t i = 0; i < toks_size; i++) {
		if (toks[i].kind == TokFunc)
			free(toks[i].Str);
	}
}

int main(int argc, char **argv) {
	if (argc != 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		fprintf(stderr, "Usage: ./exp \"<expression>\"\n");
		exit(1);
	}
	tokenize(argv[1]);
	print_toks();
	real res = eval(toks);
	printf("Result: %f\n", res);
	cleanup();
}
