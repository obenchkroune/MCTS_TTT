default:
	@c++ tic-tac-toe.cpp -Wall -Wextra -o game -lm -g3 -fsanitize=address
	@./game