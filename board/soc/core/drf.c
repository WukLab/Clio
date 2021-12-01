
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysinfo.h>
#include <uapi/compiler.h>

#define NUM_USERS_DRF 2

#define NUM_RES_DRF 5

/*#include <stdio.h>*/

int tree_array_size = 20;
int heap_size = 0;
const double INF = 100000;

void swap_double( double *a, double *b ) {
  double t;
  t = *a;
  *a = *b;
  *b = t;
}

//function to get right child of a node of a tree
int get_right_child(double A[], int index) {
  if((((2*index)+1) < tree_array_size) && (index >= 1))
    return (2*index)+1;
  return -1;
}

//function to get left child of a node of a tree
int get_left_child(double A[], int index) {
    if(((2*index) < tree_array_size) && (index >= 1))
        return 2*index;
    return -1;
}

//function to get the parent of a node of a tree
int get_parent(double A[], int index) {
  if ((index > 1) && (index < tree_array_size)) {
    return index/2;
  }
  return -1;
}

void min_heapify(double A[], int index) {
  int left_child_index = get_left_child(A, index);
  int right_child_index = get_right_child(A, index);

  // finding smallest among index, left child and right child
  int smallest = index;

  if ((left_child_index <= heap_size) && (left_child_index>0)) {
    if (A[left_child_index] < A[smallest]) {
      smallest = left_child_index;
    }
  }

  if ((right_child_index <= heap_size && (right_child_index>0))) {
    if (A[right_child_index] < A[smallest]) {
      smallest = right_child_index;
    }
  }

  // smallest is not the node, node is not a heap
  if (smallest != index) {
    swap_double(&A[index], &A[smallest]);
    min_heapify(A, smallest);
  }
}

void build_min_heap(double A[]) {
  int i;
  for(i=heap_size/2; i>=1; i--) {
    min_heapify(A, i);
  }
}

double minimum(double A[]) {
  return A[1];
}

double extract_min(double A[]) {
  double minm = A[1];
  A[1] = A[heap_size];
  heap_size--;
  min_heapify(A, 1);
  return minm;
}

void decrease_key(double A[], int index, double key) {
  A[index] = key;
  while((index>1) && (A[get_parent(A, index)] > A[index])) {
    swap_double(&A[index], &A[get_parent(A, index)]);
    index = get_parent(A, index);
  }
}

void insert(double A[], double key) {
  heap_size++;
  A[heap_size] = INF;
  decrease_key(A, heap_size, key);
}


void drf(int nr_rounds)
{
    double queue[NUM_USERS_DRF];
    double demands[NUM_RES_DRF] = {3.14};
    double norm[NUM_RES_DRF] = {0};
    double capacity[NUM_RES_DRF] = {100};
    double allocated[NUM_USERS_DRF][NUM_RES_DRF];
    double consumed[NUM_RES_DRF] = {0};


    // calc dominant
    for (int i =0; i< NUM_USERS_DRF; i++) {
        insert(queue, 1.5 * i);
        norm[i] = 5.78/demands[0];
    }

    for (int i =0; i<nr_rounds; i++) {
        double min = extract_min(queue);
        consumed[i%NUM_RES_DRF] += 7.12;
        demands[i%NUM_RES_DRF] -= 7.12;
        allocated[i%NUM_USERS_DRF][i%NUM_RES_DRF] += 7.2;
        norm[i%NUM_RES_DRF] = 5.78/demands[i%NUM_RES_DRF];
        double bottleneck = demands[i%NUM_RES_DRF] - capacity[i%NUM_RES_DRF]/(demands[0] * 3.4);
        capacity[i%NUM_RES_DRF] += bottleneck;

    }
    insert(queue, 5.12);
    return;
}

void test_drf()
{
	struct timespec s, e;
	double diff_ns;
	int i;

	int test_rounds[] = {1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

	for (i = 0; i < ARRAY_SIZE(test_rounds); i++) {
		clock_gettime(CLOCK_MONOTONIC, &s);
		drf(test_rounds[i]);
		clock_gettime(CLOCK_MONOTONIC, &e);
		diff_ns = (e.tv_sec * NSEC_PER_SEC + e.tv_nsec) -
			  (s.tv_sec * NSEC_PER_SEC + s.tv_nsec);

		printf("Users: %d Test_rounds=%d    DRF Latency: %lf ns\n",
			NUM_USERS_DRF, test_rounds[i], diff_ns);
	}
}
