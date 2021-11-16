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
	} kind;

	union {
		real Num;
		char Char;
	};
} Tok;

#define TOKS_CAP 65536
Tok toks[TOKS_CAP];
size_t toks_size = 0;

uint8_t op_prec[256] = {
	['('] = 0,
	[')'] = 0,
	['+'] = 1,
	['-'] = 1,
	['*'] = 2,
	['/'] = 2,
	['^'] = 3,
};

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

#define IS_FLOAT(c) ((c >= '0' && c <= '9') || c == '.')

void push_tok(Tok t) {
	if (toks_size+1 < TOKS_CAP)
		toks[toks_size++] = t;
}

void tokenize(char *expr) {
	push_tok((Tok){.kind = TokOp, .Char = '('});

	size_t paren_depth = 0;

	bool can_be_neg_num = true;

	char *curr = expr;
	for (char c = *curr; c != 0; c = *(++curr)) {
		if (c == ' ')
			continue;

		if (IS_FLOAT(c) ||
				(can_be_neg_num && c == '-' && IS_FLOAT(curr[1]))) {
			char buf[16];
			size_t i = 0;
			while (IS_FLOAT(curr[i]) && i < 15) {
				buf[i] = curr[i];
				i++;
			}
			curr += i - 1;
			buf[i++] = 0;

			real num = strtod(buf, NULL);

			push_tok((Tok){.kind = TokNum, .Num = num});

			can_be_neg_num = false;
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

		can_be_neg_num = c != ')';
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
		default:
			fprintf(stderr, "Error: unhandled token\n");
			exit(1);
		}
	}
	printf("\n");
}

void del_toks(Tok *begin, Tok *end) {
	memmove(begin, end, (toks_size - (end - toks)) * sizeof(Tok));
	toks_size -= end - begin;
}

real eval(Tok *t) {
	if (!(t[0].kind == TokOp && t[0].Char == '(')) {
		fprintf(stderr, "Error: expected '(' at beginning of expression\n");
		exit(1);
	}

	while (1) {
		/* Collapse parentheses. */
		if (t[1].kind == TokOp && t[1].Char == '(') {
			real res = eval(t + 1);
			size_t i;
			for (i = 2; !(t[i].kind == TokOp && t[i].Char == ')'); i++);
			del_toks(t + 1, t + i);
			/* Put the newly evaluated value into place. */
			t[1].kind = TokNum;
			t[1].Num = res;
		}
		
		if (!(t[0].kind == TokOp && t[1].kind == TokNum && t[2].kind == TokOp)) {
			fprintf(stderr, "Error: invalid token order\n");
			exit(1);
		}

		const char curr_op = t[0].Char;
		const uint8_t curr_prec = op_prec[(size_t)curr_op];

		const char next_op = t[2].Char;
		const uint8_t next_prec = op_prec[(size_t)next_op];

		if (curr_op == '(' && next_op == ')')
			return t[1].Num;

		if (next_prec > curr_prec || (next_prec == curr_prec && op_order[(size_t)curr_op] == OrderRtl)) {
			t += 2;
		} else if (next_prec < curr_prec || (next_prec == curr_prec && op_order[(size_t)curr_op] == OrderLtr)) {
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

int main(int argc, char **argv) {
	if (argc != 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		fprintf(stderr, "Usage: ./exp \"<expression>\"\n");
		exit(1);
	}
	tokenize(argv[1]);
	print_toks();
	real res = eval(toks);
	printf("Result: %f\n", res);
}
