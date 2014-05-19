#include "common.h"
#include "transform.h"
#include "io.h"
#include "uint128_t.h"
#include "nanotime.h"

const char * USAGE = "<DSK output file>";

int compare_64(const void *, const void *);
int compare_128(const void *, const void *);

int compare_64(const void * lhs, const void * rhs)
{
  uint64_t a = *(uint64_t*)lhs;
  uint64_t b = *(uint64_t*)rhs;
  if (a < b) {
    return -1;
  }
  else if (a > b){
    return 1;
  }
  return 0;
}

int compare_128(const void * lhs, const void * rhs) {
  uint64_t a_upper = ((uint64_t*)lhs)[0];
  uint64_t a_lower = ((uint64_t*)lhs)[1];
  uint64_t b_upper = ((uint64_t*)rhs)[0];
  uint64_t b_lower = ((uint64_t*)rhs)[1];
  return (a_upper - b_upper) + (a_lower - b_lower);
}

int main(int argc, char * argv[]) {
  // parse argv file
  if (argc != 2) {
    fprintf(stderr, "Usage: %s %s\n", argv[0], USAGE);
    exit(1);
  }
  char * file_name = argv[1];

  // open file
  int handle = -1;
  if ( (handle = open(file_name, O_RDONLY)) == -1 ) {
    fprintf(stderr, "Can't open file: %s\n", file_name);
    exit(1);
  }

  // read header
  uint32_t kmer_num_bits = 0;
  uint32_t k = 0;
  if ( !dsk_read_header(handle, &kmer_num_bits, &k) ) {
    fprintf(stderr, "Error reading file %s\n", file_name);
    exit(1);
  }
  uint32_t kmer_num_blocks = (kmer_num_bits / 8) / sizeof(uint64_t);
  TRACE("kmer_num_bits, k = %d, %d\n", kmer_num_bits, k);
  TRACE("kmer_num_blocks = %d\n", kmer_num_blocks);

  if (kmer_num_bits > MAX_BITS_PER_KMER) {
    fprintf(stderr, "Kmers larger than %zu bits are not currently supported. %s uses %d bits per kmer.\n",
        MAX_BITS_PER_KMER, file_name, kmer_num_bits);
    exit(1);
  }

  // read how many items there are
  size_t num_records = 0;
  if ( dsk_num_records(handle, kmer_num_bits, &num_records) == -1) {
    fprintf(stderr, "Error seeking file %s\n", file_name);
    exit(1);
  }
  if (num_records == 0) {
    fprintf(stderr, "File %s has no kmers.\n", file_name);
    exit(1);
  }
  TRACE("num_records = %zd\n", num_records);

  // ALLOCATE SPACE FOR KMERS (done in one malloc call)
  typedef uint64_t kmer_t[kmer_num_blocks];
  kmer_t * kmers = calloc(num_records * 2, sizeof(kmer_t));

  // READ KMERS FROM DISK INTO ARRAY
  size_t num_records_read = num_records_read = dsk_read_kmers(handle, kmer_num_bits, (uint64_t*) kmers);
  switch (num_records_read) {
    case -1:
      fprintf(stderr, "Error reading file %s\n", argv[1]);
      exit(1);
      break;
    default:
      TRACE("num_records_read = %zu\n", num_records_read);
      assert (num_records_read == num_records);
      break;
  }

  close(handle);

  #ifndef NDEBUG
  print_kmers_hex(stdout, (uint64_t*)kmers, num_records, kmer_num_bits);
  #endif

  // TODO: TRANSFORM to have LSB at MSB position for sorting

  /*
  #ifndef NDEBUG
  TRACE("TESTING REVERSE COMPLEMENTS\n");
  if (kmer_num_bits == 64) {
    uint64_t x = ((uint64_t*)kmers)[0];
    uint64_t y = reverse_complement_64(x, k);
    TRACE("     x  = %016llx\n", x);
    TRACE("  rc(x) = %016llx\n", y);
  }
  else if (kmer_num_bits == 128) {
    uint128_struct x = ((uint128_struct*)kmers)[0];
    uint128_struct y = (uint128_struct)reverse_complement_128(x, k);
    TRACE("     x  = %016llx %016llx\n", x.upper, x.lower);
    TRACE("  rc(x) = %016llx %016llx\n", y.upper, y.lower);
  }
  #endif
  */

  if (kmer_num_bits == 64) {
    nanotime_t start, end;
    start = get_nanotime();
    qsort(kmers, num_records, sizeof(uint64_t), compare_64);
    end = get_nanotime();
    double ms = (double)(end - start)/(1000000);
    fprintf(stderr, "QSORT timing: %.2f ms\n", ms);
  }
  else if (kmer_num_bits == 128) {
    // TODO: Fix compare_128
    qsort(kmers, num_records, sizeof(uint128_t), compare_128);
  }

  // Store a copy of the kmers to sort in colex(row) order, for joining
  // in order to calculate dummy edges
  kmer_t * table_a = kmers;
  kmer_t * table_b = kmers + num_records * 2; // x2 because of reverse complements
  memcpy(table_b, table_a, num_records * 2 * sizeof(kmer_t));

  // TODO: change buffer size to 2x (then to 4x)
  // convert, then reverse all 2-bit fields
  // add reverse complements to second half of array
  // Use quicksort to sort them all
  // Memcpy, transform and resort - insertion sort based on last field?
  // filter dummy edges in two passes, keeping a list of positions
  // update positions so we can write them out
  // write out and keep edge counter, compare to dummy edge positions
  // - fill output buffer, each entry takes 5 bits of a 64 bit int
  // Find a server to run it on
  // compare time vs minia for SAME K VALUE
  // 2 data sets
  free(kmers);

  return 0;
}
