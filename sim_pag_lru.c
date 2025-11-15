
/*
    Copyright 2023 The Operating System Group at the UAH
    sim_pag_lru.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./sim_paging.h"

// Function that initialises the tables

void init_tables(ssystem* S) {
  int i;

  // Reset pages
  memset(S->pgt, 0, sizeof(spage) * S->numpags);

  // Empty LRU stack
  S->lru = -1;

  // Reset LRU(t) time
  S->clock = 0;

  // Circular list of free frames
  for (i = 0; i < S->numframes - 1; i++) {
    S->frt[i].page = -1;
    S->frt[i].next = i + 1;
  }

  S->frt[i].page = -1;  // Now i == numframes-1
  S->frt[i].next = 0;   // Close circular list
  S->listfree = i;      // Point to the last one

  // Empty circular list of occupied frames
  S->listoccupied = -1;
}

// Functions that simulate the hardware of the MMU

unsigned sim_mmu(ssystem* S, unsigned virtual_addr, char op) {
    unsigned physical_addr;
    int page, frame, offset;

    page   = virtual_addr / S->pagsz;   // Cociente
    offset = virtual_addr % S->pagsz;   // Resto

    // Comprobar referencia fuera de rango
    if (page < 0 || page >= S->numpags) {
        S->numillegalrefs++;    // Contador de referencias ilegales
        return ~0U;             // Dirección física inválida (0xFFFF..)
    }

    // Si la página no está presente, provocar fallo de página
    if (!S->pgt[page].present)
        handle_page_fault(S, virtual_addr);

    // Ahora ya está presente
    frame = S->pgt[page].frame;
    physical_addr = frame * S->pagsz + offset;

    // Simular la referencia (contadores, modified, etc.)
    reference_page(S, page, op);

    if (S->detailed) {
        printf("\t%c %u == P%d(F%d) + %d\n",
               op, virtual_addr, page, frame, offset);
    }

    return physical_addr;
}

void reference_page(ssystem* S, int page, char op) {
  if (op == 'R') {              // If it's a read,
    S->numrefsread++;           // count it
  } else if (op == 'W') {       // If it's a write,
    S->pgt[page].modified = 1;  // count it and mark the
    S->numrefswrite++;          // page 'modified'+

    S->pgt[page].clock = S->clock;
    S->clock++;

    if (S->clock == 0) {   // overflow natural del unsigned
        if (S->detailed) printf("@ WARNING: clock overflow! Normalizing timestamps...\n");
    }

  }
}

// Functions that simulate the operating system

void handle_page_fault(ssystem* S, unsigned virtual_address) {
  // TODO(student):
  //       Type in the code that simulates the Operating
  //
    int page, victim, frame, last;

    S->numpagefaults ++;
    page = virtual_address / S-> pagsz;
    if (S->detailed) {
        printf ("@ PAGE_FAULT in P %d!\n", page);
    }

    if (S->listfree != -1) {
	// There are free frames
        last = S->listfree;
        frame = S->frt[last].next;
        if (frame==last) {
            //el ultimo frame tiene en su puntero a siguiente a si mismo, es decir, se apunta a si mismo.
            // Then, this is the last one left.
            S->listfree = -1; //se consume y ya no quedan libres.
        } else {
            // Otherwise, bypass
            S->frt[last].next = S->frt[frame].next;
            //ahora el ultima apunta al segundo libre que había, ha saltado al primero(frame).
        }
        occupy_free_frame(S, frame, page);
    }else {
	// There are not free frames
        victim = choose_page_to_be_replaced(S);
        replace_page(S, victim, page);
    }
}


int choose_page_to_be_replaced(ssystem* S) {
  int frame, victim;

  victim = S->frt[0].page;
  frame=0;
  int lowestTimestamp = S->pgt[victim].timestamp;

  int pageAux;

  for(int i = 1; i< S->numframes; i++){
    pageAux = S->frt[i].page;
    if(lowestTimestamp>S->pgt[pageAux].timestamp){
        //nuevo menor
        lowestTimestamp = S->pgt[pageAux].timestamp;
        victim = pageAux;
        frame=i;
    }
  }

  if (S->detailed) printf("@ LRU chooses P%d in F%d (ts=%d)\n", victim, frame, lowestTimestamp);

  return victim;
}

void replace_page(ssystem* S, int victim, int newpage) {
  int frame;

  frame = S->pgt[victim].frame;

  if (S->pgt[victim].modified) {
    if (S->detailed)
      printf(
          "@ Writing modified P%d back (to disc) to "
          "replace it\n",
          victim);

    S->numpgwriteback++;
  }

  if (S->detailed)
    printf("@ Replacing victim P%d with P%d in F%d\n", victim, newpage, frame);

  S->pgt[victim].present = 0;

  S->pgt[newpage].present = 1;
  S->pgt[newpage].frame = frame;
  S->pgt[newpage].modified = 0;

  S->frt[frame].page = newpage;
}

void occupy_free_frame(ssystem* S, int frame, int page) {

    if (S->detailed)
        printf("@ Storing P%d in F%d\n", page, frame);

    // 1. Actualizar la tabla de páginas
    S->pgt[page].frame     = frame;
    S->pgt[page].present   = 1;
    S->pgt[page].modified  = 0;    // al cargarse no está modificada
    S->pgt[page].referenced = 0;   // aun no se ha referenciado, solo se ha cargado. En otro momento se referenciará

    // 2. Actualizar la tabla de frames
    S->frt[frame].page = page;

    // RANDOM REPLACEMENT:
    // No se usa listoccupied, no se toca la lista

  // TODO(student):
  //       Write the code that links the page with the frame and
  //       vice-versa, and wites the corresponding values in the
  //       state bits of the page (presence...)
}

// Functions that show results

void print_page_table(ssystem* S) {
  int p;

  printf("%10s %10s %10s %10s  %10s\n", "PAGE", "Present", "Frame", "Modified", "Timestamp");

  for (p = 0; p < S->numpags; p++)
    if (S->pgt[p].present)
      printf("%8d   %6d     %8d   %6d  %6u\n", p, S->pgt[p].present, S->pgt[p].frame,
             S->pgt[p].modified, S->pgt[p].timestamp);
    else
      printf("%8d   %6d     %8s   %6s  %6s\n", p, S->pgt[p].present, "-", "-", "-");
}

void print_frames_table(ssystem* S) {
  int p, f;

  printf("%10s %10s %10s %10s %10s\n", "FRAME", "Page", "Present", "Modified", "Timestamp");

  for (f = 0; f < S->numframes; f++) {
    p = S->frt[f].page;

    if (p == -1)
      printf("%8d   %8s   %6s     %6s    %4s\n", f, "-", "-", "-", "-");
    else if (S->pgt[p].present)
      printf("%8d   %8d   %6d     %6d    %6u\n", f, p, S->pgt[p].present,
             S->pgt[p].modified, S->pgt[p].timestamp);
    else
      printf("%8d   %8d   %6d     %6s   ERROR!\n", f, p, S->pgt[p].present,
             "-");
  }
}

void print_replacement_report(ssystem* S) {
    int lowf = 0, highf = 0;
    int paux = S->frt[0].page, lowp=paux , highp=paux;
    unsigned lowt= S->pgt[lowp].timestamp, hight = lowt;

  for (int f = 1; f < S->numframes; f++) {
      paux = S->frt[f].page;
      if(S->pgt[paux].timestamp > hight){
        hight = S->pgt[paux].timestamp;
        highf=f;
        highp=paux;
      }else if(S->pgt[paux].timestamp < lowt){
        lowt = S->pgt[paux].timestamp;
        lowf=f;
        lowp=paux;
      }
  }
  printf("LRU replacement\n"
         "lowest timestamp = %u in frame %d  (page %d)\n"
         "highest timestamp = %u  in frame %d  (page %d)\n",
         lowt, lowf, lowp, hight, highf,highp);
}
