all: dev serv

# make rule per il client
dev: device.o IOMultiplex.o cmd.o network.o
	gcc -Wall device.o IOMultiplex.o cmd.o network.o -o dev

device.o: device.c
	gcc -c -g device.c

# make rule per il server
serv: server.o IOMultiplex.o cmd.o network.o
	gcc -Wall server.o IOMultiplex.o cmd.o network.o -o serv

server.o: server.c
	gcc -c -g server.c

IOMultiplex.o: util/IOMultiplex.c
	gcc -c -g util/IOMultiplex.c

cmd.o: util/cmd.c
	gcc -c -g util/cmd.c

network.o: util/network.c
	gcc -c -g util/network.c

# pulizia dei file della compilazione 
clean: 
	rm *.exe *.o dev serv

# sezione dedicata al testing del codice in fase di sviluppo

Here=$(PWD)

# cartella contenente i file di test
Testdir=$(Here)/tests

# cartella temporanea per contenere i risultati effettivi .got
Tmp=$(Testdir)/tmp

ready:  $(Testdir) $(Tmp)

Tests:=$(shell cd $(Testdir); ls  | egrep '^[0-9]+$$' | sort -n  )

# esecuzione di uno specifico tesst (esempio: make X=001 run)
run : ready $(Testdir)/$(X)
	@echo $X 2>&1
	@cat $(Testdir)/$(X) | $(Run)

# salvataggio nel file cache .want del risultato effettivo del test (esempio: make X=001 cache)
cache : ready
	@$(MAKE) -s run | tee $(Testdir)/$X.want
	@echo new test result cached to $(Testdir)/$X.want

# confronto del risultato del test salvato nel file .got 
# con il risultato atteso contenuto in nel file .want (esempio: make X=001 test)
test : ready $(Testdir)/$(X).want
	@$(MAKE) -s run > $(Tmp)/$X.got
	@if diff -s $(Tmp)/$X.got $(Testdir)/$X.want > /dev/null;  \
		then echo PASSED $X ; \
		else echo FAILED $X,  got $(Tmp)/$X.got; \
            diff $(Testdir)/$X.want $(Tmp)/$X.got; \
		fi

# effettua tutti i test contenuti nella cartella $(Testdir) (esempio: make test-all)
test-all:  
	@echo $(Tests)
	@$(foreach x, $(Tests), $(MAKE) -s X=$x test;)

# salvataggio in cache dei risultati attesi per tutti i test contenuti in $(Testdir)
# da usare con cautela, in quanto verranno sovrascritti tutti i risultati attesi salvati in cache
cache-all :
	@echo Affected files: $(Tests)
	@echo If you are absolutely sure, use make cache-all-I-know-what-Im-doing

# implementazione effettiva dell'istruzione cache-all
cache-all-I-know-what-Im-doing :
	@$(foreach x, $(Tests), $(MAKE) -s X=$x cache;)

# conteggio dei test superati e falliti
score :
	@$(MAKE) -s test-all | cut -d\  -f 1 | egrep '(PASSED|FAILED)' | sort | uniq -c

# comando per mettere effettivamente in esecuzione il test passato come paramentro
# varia in base al linguaggio (in questo caso, gli script bash vengono semplicemente lanciati)
#Run=echo got $(1); bash -c "${1}" #; ./$(2)
Run = sh
