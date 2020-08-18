typedef uint64_t remote_ptr;

const int valueSize = 128;

struct linkedlist {
    uint64_t   key;
    remote_ptr value;
    remote_ptr next;
};
void test() {

    // prepare data and value
    uint64_t keybase = 0xabcd0000;
    int length = 128;
    remote_ptr head = alloc(sizeof(struct linkedlist));
    remote_ptr cur = head;
    for (int i = 0; i < length; i++) {
        struct linkedlist ll = {
            .key = keybase + i,
            .value = 0,
            .next = 0,
        };
        if (i != length - 1) {
            ll.next = alloc(sizeof(struct linkedlist));
        } else {
            ll.value = alloc(valueSize);
            // Write value here
        }
        write(cur, ll);
        cur = ll.next;
    }

    // call pointer chasing
    int ret = legomem_pointer_chasing(ctx,
		      head, keybase + length - 1, // Find the last entry
		      sizeof(struct linkedlist), // uint16_t structSize, size of the struct in bytes
              valuesize, // uint16_t valueSize, size of value
		      0, // uint8_t keyOffset, offset of key in struct, in bytes
              8, // uint8_t valueOffset, offset of value 
              0, // uint8_t depth, not used
		      16, // uint8_t nextOffset, offset of next, inbytes
              );
    // As in the API, will return an read_resp

}
