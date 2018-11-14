all:
	cc -std=c99 -g lispy.c mpc.c -lm -ledit -o lispy

solution: 
	cc -std=c99 -g expr_solution.c mpc.c -lm -ledit -o expr_solution

clean:
	rm *.out
