/*
tm_regex.h v0.0.2 - public domain - https://github.com/to-miz/tm
author: Tolga Mizrak 2016

no warranty; use at your own risk

NOTES
    Simple DFA based minimalist regex matcher.
    Works by going from regex pattern to NFA to DFA.
    No fancy features, just linear time regex matching.

LICENSE
    see license notes at end of file

HISTORY
    v0.0.2  25.08.18 implemented grouping
    v0.0.1  2016     first version
*/

#ifndef _TM_REGEX_H_INCLUDED_
#define _TM_REGEX_H_INCLUDED_

#ifdef __cplusplus
  #include <cassert>
  #include <cstddef>
  #include <cstdint>
  #include <cstdio>
  #include <cstring>
#else
  #include <assert.h>
  #include <stddef.h>
  #include <stdint.h>
  #include <stdio.h>
  #include <string.h>
#endif

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef int32_t int32;
typedef uint32_t uint32;

typedef int16_t index_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NST_ENTRY,
  NST_SPLIT,
  NST_EMTPY,
  NST_MATCH,
} NfaStateType;

typedef struct {
  uint32 cp;
} RegexEntry;

typedef struct {
  int8 type;
  int16 group;
  union {
    RegexEntry entry;
    index_t next2;
  };
  index_t next;
} NfaState;

typedef struct {
  NfaState* data;
  size_t size;
  size_t capacity;
} NfaStatePool;

typedef struct {
  index_t index;
  int16 group;
} DfaEdge;

typedef struct {
  RegexEntry* entries;
  DfaEdge* next;
  int8 matching;
  size_t count;
} DfaState;

typedef struct {
  DfaState* data;
  size_t size;
  size_t capacity;
} DfaStatePool;

typedef struct {
  RegexEntry* data;
  size_t size;
  size_t capacity;
} RegexEntryPool;

typedef struct {
  DfaEdge* data;
  size_t size;
  size_t capacity;
} DfaEdgePool;

typedef struct {
  NfaStatePool pool;
  index_t start;
} RegexNfaContext;

typedef struct {
  DfaStatePool pool;
  RegexEntryPool entryPool;
  DfaEdgePool edgePool;
} RegexDfaContext;

uint32 utf8NextCodepoint(const char** str, size_t* remaining) {
  --*remaining;
  return (uint8)(*((*str)++));
}

index_t addState(RegexNfaContext* context, int8 type, int16 group) {
  index_t ret = (index_t)(context->pool.size);
  ++context->pool.size;
  NfaState* state = &context->pool.data[ret];
  state->type = type;
  state->group = group;
  state->next = -1;
  return ret;
}

index_t regexToNfa(RegexNfaContext* context, const char* str, size_t size) {
  typedef struct {
    index_t prev;
    index_t tail;
    index_t head;
  } StackNode;
  StackNode stackFirst[100];
  StackNode* stack = stackFirst;
  NfaState* pool = context->pool.data;

  int16 group = 0;
  stack->head = stack->tail = addState(context, NST_MATCH, -1);
  stack->prev = stack->head;
  pool[stack->head].next = stack->tail;

  index_t modifyHead = -1;
  while (size) {
    uint32 cp = utf8NextCodepoint(&str, &size);
    index_t index;
    NfaState* state;
    NfaState* prev = &pool[stack->prev];
    switch (cp) {
      case '|': {
        modifyHead = -1;
        ++group;
        index = addState(context, NST_SPLIT, group);
        state = &pool[index];
        state->next2 = stack->head;
        stack->head = index;
        break;
      }
      case '+': {
        if (modifyHead < 0) {
          return -1;
        }
        index = addState(context, NST_SPLIT, group);
        state = &pool[index];
        state->next2 = modifyHead;
        prev->next = index;
        break;
      }
      case '*': {
        if (modifyHead < 0) {
          return -1;
        }
        index = modifyHead;
        state = &pool[index];
        prev->next = index;
        index_t copyIndex = addState(context, state->type, group);
        pool[copyIndex] = *state;
        state->type = NST_SPLIT;
        state->next2 = copyIndex;
        break;
      }
      case '?': {
        if (modifyHead < 0) {
          return -1;
        }
        index = modifyHead;
        state = &pool[index];
        index_t copyIndex = addState(context, state->type, group);
        pool[copyIndex] = *state;
        state->type = NST_SPLIT;
        state->next2 = copyIndex;
        break;
      }
      case '(': {
        modifyHead = -1;
        StackNode* current = stack;
        ++stack;
        ++group;
        stack->head = stack->tail = addState(context, NST_EMTPY, group);
        stack->prev = stack->head;
        pool[stack->tail].next = current->tail;
        continue;
      }
      case ')': {
        if (stack <= stackFirst) {
          return -1;
        }
        StackNode* current = stack;
        --stack;
        prev = &pool[stack->prev];
        state = &pool[current->tail];
        prev->next = current->head;
        index = current->tail;
        if (stack->head == stack->tail) {
          stack->head = current->head;
        }
        modifyHead = current->head;
        break;
      }
      default: {
        index = addState(context, NST_ENTRY, group);
        state = &pool[index];
        state->entry.cp = cp;
        prev->next = index;
        if (stack->head == stack->tail) {
          stack->head = index;
        }
        modifyHead = index;
        break;
      }
    }
    state->next = stack->tail;
    stack->prev = index;
  }

  if (stack != stackFirst) {
    return -1;
  }
  context->start = stack->head;
  return stack->head;
}

typedef struct {
  index_t index;
  int16 group;
} StateListEntry;

typedef struct {
  StateListEntry* data;
  size_t size;
  size_t capacity;
} StateList;

void addStateToList(StateList* list, NfaState* pool, index_t index,
                    int16 group) {
  assert(index >= 0);
  NfaState* current = &pool[index];
  int8 nextGroup = (group > 0) ? group : current->group;
  switch (current->type) {
    case NST_SPLIT: {
      addStateToList(list, pool, current->next2, -1);
      addStateToList(list, pool, current->next, -1);
      return;
    }
    case NST_EMTPY: {
      int8 currentGroup = (current->group > 0) ? current->group : group;
      addStateToList(list, pool, current->next, currentGroup);
      return;
    }
  }
  list->data[list->size++] = {index, nextGroup};
}
void initList(StateList* list, NfaState* pool, index_t index) {
  list->size = 0;
  addStateToList(list, pool, index, -1);
}
int stepList(StateList* current, StateList* next, NfaState* pool, uint32 cp) {
  next->size = 0;
  size_t count = current->size;
  int matched = 0;
  for (size_t i = 0; i < count; ++i) {
    NfaState* state = &pool[current->data[i].index];
    if (state->type == NST_ENTRY && state->entry.cp == cp) {
      addStateToList(next, pool, state->next, state->group);
      matched = 1;
    }
  }
  return matched;
}
int isMatch(StateList* list, NfaState* pool) {
  size_t count = list->size;
  for (size_t i = 0; i < count; ++i) {
    StateListEntry listEntry = list->data[i];
    NfaState* entry = &pool[listEntry.index];
    if (entry->type == NST_MATCH) {
      return 1;
    }
  }
  return 0;
}
int match(RegexNfaContext* context, const char* str, size_t size) {
  StateListEntry l1[100];
  StateListEntry l2[100];
  StateList current = {l1, 0, 100};
  StateList next = {l2, 0, 100};
  NfaState* pool = context->pool.data;

  initList(&current, pool, context->start);
  while (size) {
    uint32 cp = utf8NextCodepoint(&str, &size);
    if (!stepList(&current, &next, pool, cp)) {
      return -1;
    }
    StateList temp = current;
    current = next;
    next = temp;
  }
  if(isMatch(&current, pool)) {
    // return the edge that
    return next.data[0].group;
  }
  return -1;
}

int matchingStates(StateList* a, StateList* b) {
  if (a->size != b->size) {
    return 0;
  }
  for (size_t i = 0; i < a->size; ++i) {
    int matched = 0;
    for (size_t j = 0; j < b->size; ++j) {
      if (a->data[i].index == b->data[i].index) {
        matched = 1;
        break;
      }
    }
    if (!matched) {
      return 0;
    }
  }
  return 1;
}
void visitNfa(StateList* states, NfaState* pool, index_t index) {
  NfaState* current = &pool[index];
  switch (current->type) {
    case NST_SPLIT: {
      visitNfa(states, pool, current->next2);
      visitNfa(states, pool, current->next);
      return;
    }
    case NST_EMTPY: {
      visitNfa(states, pool, current->next);
      return;
    }
  }
  states->data[states->size++] = {index, current->group};
}
void makeDfa(RegexNfaContext* nfaContext, RegexDfaContext* dfaContext) {
  StateListEntry l1[100];
  StateListEntry* indices = l1;
  StateList statesFirst[100];
  StateList* states = statesFirst;
  NfaState* pool = nfaContext->pool.data;
  DfaState* nodes = dfaContext->pool.data;
  DfaState* nodesFirst = dfaContext->pool.data;
  DfaEdge* indexPtr = dfaContext->edgePool.data;
  RegexEntry* entryPtr = dfaContext->entryPool.data;

  *states = {l1, 0, 100};
  visitNfa(states, pool, nfaContext->start);
  DfaState* current = nodes;
  *current = {};
  indices += states->size;
  StateList* currentState = states;
  do {
    current->entries = entryPtr;
    current->next = indexPtr;
    current->matching = 0;
    for (size_t i = 0; i < currentState->size; ++i) {
      NfaState* state = &pool[currentState->data[i].index];
      switch (state->type) {
        case NST_ENTRY: {
          ++states;
          *states = {indices, 0, 100};
          stepList(currentState, states, pool, state->entry.cp);

          index_t found = -1;
          for (StateList* other = statesFirst; other < states; ++other) {
            if (matchingStates(other, states)) {
              found = (index_t)(other - statesFirst);
              break;
            }
          }
          if (found >= 0) {
            --states;
          } else {
            indices += states->size;
            DfaState* added = ++nodes;
            added->next = 0;
            added->entries = 0;
            added->count = 0;
            found = (index_t)(nodes - nodesFirst);
          }
          ++entryPtr;
          ++indexPtr;
          current->entries[current->count].cp = state->entry.cp;
          current->next[current->count] = {found, state->group};
          ++current->count;
          break;
        }
        case NST_MATCH: {
          current->matching = 1;
          break;
        }
        default: {
          assert(0);
          break;
        }
      }
    }
    ++current;
    ++currentState;
  } while (current <= nodes);

  for (StateList* entry = statesFirst; entry <= states; ++entry) {
    for (size_t i = 0; i < entry->size; ++i) {
      printf("%d ", (int)entry->data[i].index);
    }
    printf("\n");
  }
}

int dfaMatch(RegexDfaContext* context, const char* str, size_t size) {
  DfaState* pool = context->pool.data;
  DfaState* state = pool;
  int16 group = -1;
  while (size) {
    uint32 cp = utf8NextCodepoint(&str, &size);
    int matched = 0;
    for (size_t i = 0; i < state->count; ++i) {
      if (state->entries[i].cp == cp) {
        matched = 1;
        if(state->next[i].group >= 0) {
          group = state->next[i].group;
        }
        state = &pool[state->next[i].index];
        break;
      }
    }
    if (!matched) {
      return -1;
    }
  }
  printf("Matched state: %d\n", (int)(state - pool));
  return state->matching ? group : -1;
}

#ifdef __cplusplus
}
#endif

#endif  // _TM_REGEX_H_INCLUDED_

/*
There are two licenses you can freely choose from - MIT or Public Domain
---------------------------------------------------------------------------

MIT License:
Copyright (c) 2016 Tolga Mizrak

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---------------------------------------------------------------------------

Public Domain (www.unlicense.org):
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

---------------------------------------------------------------------------
*/