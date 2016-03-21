INCLUDE_PATH=/usr/local/include
LIBRARY_PATH=/usr/local/lib
CC_OPTS=-O2

#all:	dive rows_cols
all:	dive

rows_cols: rows_cols.c
	gcc rows_cols.c -o rows_cols $(CC_OPTS) -I$(INCLUDE_PATH) -L$(LIBRARY_PATH) -lbeagleboneio

dive:	dive.c
	gcc dive.c -o dive $(CC_OPTS) -I$(INCLUDE_PATH) -L$(LIBRARY_PATH) -lbeagleboneio


