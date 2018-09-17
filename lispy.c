#include "mpc.h" 

#ifdef _WIN32

static char buffer[2048];


char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1); // for the null char which terminates the string 
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0'; // see?
    return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Add QEXPR as possible lval type */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

typedef struct lval {
    int type;
    double num;
    char* err;
    char* sym;
    int count;
    struct lval** cell;
} lval;

// helper for creating an numerical lval
lval* lval_num(double x) {
    lval* v = malloc(sizeof(lval)); // c way, they must be declared prior to use. declaration is resource acquisition.
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// helper for creating an error lval
lval* lval_err(char* e) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(e) + 1);
    strcpy(v->err, e);
    return v;
}

// helper for making a symbol lval
lval* lval_sym(char* sym) {
    lval* v = malloc(sizeof(lval));
    v->sym = malloc(strlen(sym) + 1);
    v->type = LVAL_SYM;
    strcpy(v->sym, sym);
    return v;
}

// you guessed it, an S-expression (symbolic expression)
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0; 
    v->cell = NULL;
    return v;
}


// a Q-Expression: a quoted expression.
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// deletes the lval. 
// if we're mallocing, we must delete.
void lval_delete(lval* v) {
    switch(v->type) {
        case LVAL_NUM: break; // no malloc was done for number
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
          for (int i = 0; i < v->count; i++) {
              lval_delete(v->cell[i]);
          }
          free(v->cell);
        break;
    }
    free(v); // free the top level struct itself, as it will be malloc'd.
}

// forward declaring this function to break cyclical dependency.
void lval_print(lval* v);

void lval_print_expr(lval* v, char open, char close) {
    printf("call to print_expr \n");
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        if (i != (v->count - 1)) putchar(' ');
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_NUM:
            printf("%lf", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            printf("printing sexpr\n");
            lval_print_expr(v, '(', ')'); 
            break;
        case LVAL_QEXPR:
            lval_print_expr(v, '{', '}');
            break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

double max(double a, double b) {
    if (a > b) return a;
    else return b;
}

lval* eval_op(lval* x, char* op, lval* y) {
    if (x->type == LVAL_ERR) { return x; }
    if (y->type == LVAL_ERR) { return y; }

    // mathematical operations
    if (strcmp(op, "+") == 0) { return lval_num(x->num + y->num); }
    if (strcmp(op, "-") == 0) { return lval_num(x->num - y->num); }
    if (strcmp(op, "*") == 0) { return lval_num(x->num * y->num); }
    if (strcmp(op, "/") == 0) { 
        if (y->num == 0) {
            return lval_err("division or modulo by zero");
        } else {
        return lval_num(x->num / y->num); 
        }
    }
    return lval_err("unrecognized operator");
}



lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    double d = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_num(d) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    // Also, this is the base case of the recursion. Namely, the case in which
    // the s-expr (or the abstract syntax subtree) evaluates to just one number.
    // some basic error checking to see if we have a valid number.
    if (strstr(t->tag, "number")) {
        return lval_read_num(t);
    }

    if (strstr(t->tag, "symbol")) {
        return lval_sym(t->contents);
    }

    lval* x = NULL;
    
    if (strcmp(t->tag, ">") == 0) {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); } // root or sexpr

    for (int i = 0; i < t->children_num; i++) {
        char* contents = t->children[i]->contents;
        if (strcmp(contents, "(") == 0 || 
            strcmp(contents, ")") == 0 || 
            strcmp(contents, "{") == 0 || 
            strcmp(contents, "}") == 0 || 
            strcmp(t->children[i]->tag, "regex") == 0) {
                printf("continue the for loop: %d\n", i);
                continue;
            }
        printf("adding lval. tag:%s\n", t->children[i]->tag);
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}



int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
      "                                          \
        number : /-?([0-9]*['.'])?[0-9]+/ ;      \
        symbol : '+' | '-' | '*' | '/' ;         \
        sexpr  : '(' <expr>* ')' ;               \
        qexpr  : '{' <expr>* '}' ;              \
        expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
        lispy  : /^/ <expr>* /$/ ;               \
      ",
      Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

     puts("Lispy v0.5");
     puts("Press Ctrl+C to Exit\n");

     while(1) {
         char* input = readline("lispy> ");

         add_history(input);

         mpc_result_t r;
         if (mpc_parse("<stdin>", input, Lispy, &r)) {
             lval* result = lval_read(r.output);
             mpc_ast_print(r.output);
             lval_println(result);
             lval_delete(result);
             mpc_ast_delete(r.output);
         } else {
             printf("unable to parse stdin\n");
             mpc_err_print(r.error);
             mpc_err_delete(r.error);
         }

         free(input);
     }

     mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
     return 0;
}
