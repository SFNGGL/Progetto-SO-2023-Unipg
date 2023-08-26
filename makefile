NAME_EXE := cena
CODE := philosophers
LIBRARY := supper
FLAGS := -lrt -pthread
CC := gcc

.PHONY: clean

all : $(CODE)

$(CODE): $(CODE).c $(LIBRARY).h
	${CC} -c $(CODE).c 
	${CC} -c $(LIBRARY).h
	${CC} -o $(NAME_EXE) $(CODE).c $(LIBRARY).h $(FLAGS)

clean:
	@rm -rf $(NAME_EXE)
	@echo "Rimosso l'eseguibile"

help:
	@echo "Sintassi: ./cena <Numero> <Flag>"
	@echo "Numero: intero compreso tra 0 e 255"
	@echo "Flag valide: -ndabh (una sola di queste alla volta)"

# remove:
# 	@rm -rf $(NAME_EXE) $(CODE).c $(LIBRARY).h
# 	@echo "Rimossi tutti i file"
