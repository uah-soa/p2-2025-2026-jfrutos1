/*
    Copyright 2023 The Operating System Group at the UAH
    sim_pag_fifo.c
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
    S->numrefswrite++;          // page 'modified'
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

  int frame = S->frt[S->listoccupied].next;

  if (S->detailed)
    printf(
        "@ Choosing by FIFO P%d of F%d to be "
        "replaced\n",
        S->frt[frame].page, frame);

  return S->frt[frame].page;
}

void replace_page(ssystem* S, int victim, int newpage) {

  int frame = S->pgt[victim].frame;

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
  S->pgt[newpage].referenced=0;

  S->frt[frame].page = newpage;

  S->frt[frame].next            = S->frt[S->listoccupied].next; // primero actual
  S->frt[S->listoccupied].next  = frame;                        // último -> nuevo
  S->listoccupied               = frame;                        // nuevo último
    }
}

void occupy_free_frame(ssystem* S, int frame, int page) {

    if (S->detailed)
        printf("@ Storing P%d in F%d\n", page, frame);

    // 1. Actualizar la tabla de páginas
    S->pgt[page].frame      = frame;
    S->pgt[page].present    = 1;
    S->pgt[page].modified   = 0;
    S->pgt[page].referenced = 0;

    // 2. Actualizar la tabla de frames
    S->frt[frame].page = page;

    // 3. Insertar el frame en la lista de ocupados (FIFO)
    if (S->listoccupied == -1) {
        // Lista vacía: el frame se apunta a sí mismo
        S->frt[frame].next = frame;
        S->listoccupied = frame;
    } else {
        // Lista NO vacía: insertar detrás del último
        S->frt[frame].next            = S->frt[S->listoccupied].next; // primero actual
        S->frt[S->listoccupied].next  = frame;                        // último -> nuevo
        S->listoccupied               = frame;                        // nuevo último
    }
}


// Functions that show results

void print_page_table(ssystem* S) {
  int p;

  printf("%10s %10s %10s   %s\n", "PAGE", "Present", "Frame", "Modified");

  for (p = 0; p < S->numpags; p++)
    if (S->pgt[p].present)
      printf("%8d   %6d     %8d   %6d\n", p, S->pgt[p].present, S->pgt[p].frame,
             S->pgt[p].modified);
    else
      printf("%8d   %6d     %8s   %6s\n", p, S->pgt[p].present, "-", "-");
}

void print_frames_table(ssystem* S) {
  int p, f;

  printf("%10s %10s %10s   %s\n", "FRAME", "Page", "Present", "Modified");

  for (f = 0; f < S->numframes; f++) {
    p = S->frt[f].page;

    if (p == -1)
      printf("%8d   %8s   %6s     %6s\n", f, "-", "-", "-");
    else if (S->pgt[p].present)
      printf("%8d   %8d   %6d     %6d\n", f, p, S->pgt[p].present,
             S->pgt[p].modified);
    else
      printf("%8d   %8d   %6d     %6s   ERROR!\n", f, p, S->pgt[p].present,
             "-");
  }
}

void print_replacement_report(ssystem* S) {
    if (S->listoccupied == -1) {
        printf("FIFO replacement: no occupied frames.\n");
        return;
    }

    int victim_frame = S->frt[S->listoccupied].next;
    int victim_page  = S->frt[victim_frame].page;

    printf("FIFO replacement\n");
    printf("Next victim will be: frame %d (page %d)\n",
           victim_frame, victim_page);
}
