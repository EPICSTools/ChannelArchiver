#include <string.h>
#include "bin_io.h"
#include "file_allocator.h"

// Layout of the data file:
//
// - From the beginning of the file, reserved_space
//   bytes are skipped.
// - Two list_node entries follow:
//   a) list of allocated units,
//   b) list of free units
// - Allocated or free data block follow:
//   each one starts with a list_node,
//   the user of the file_allocator is handed
//   the offset to the next byte

// We read/write the contents ourself,
// not including possible pad bytes of the structure
static const long list_node_size = 12;

// To avoid allocating tiny areas,
// also to avoid splitting free blocks into pieces
// that are then too small to be ever useful,
// all memory regions are at least this big:
static const long minimum_size = 64;

file_allocator::file_allocator()
{
    f = 0;
}

file_allocator::~file_allocator()
{
    if (f)
        fprintf(stderr, "file_allocator wasn't detached\n");
}

bool file_allocator::attach(FILE *f, long reserved_space)
{
    this->f = f;
    this->reserved_space = reserved_space;
    if (fseek(this->f, 0, SEEK_END))
        return false;
    file_size = ftell(this->f);
    if (file_size < 0)
        return false;
    // File size should be
    // 0 for new file or at least reserved_space + list headers
    if (file_size == 0)
    {
        // create empty list headers
        memset(&allocated_head, 0, list_node_size);
        memset(&free_head, 0, list_node_size);
        if (!write_node(this->reserved_space, &allocated_head) ||
            !write_node(this->reserved_space+list_node_size, &free_head))
            return false;
        file_size = ftell(this->f);
        return file_size == reserved_space + 2*list_node_size;
    }
    if (file_size < this->reserved_space + 2*list_node_size)
    {
        fprintf(stderr, "file_allocator: File contains broken header\n");
        return false;
    }
    // read existing list headers
    return 
        read_node(this->reserved_space, &allocated_head) &&
        read_node(this->reserved_space+list_node_size, &free_head);
}

void file_allocator::detach()
{
    f = 0;
}

long file_allocator::allocate(long num_bytes)
{
    if (num_bytes < minimum_size)
        num_bytes = minimum_size;
    list_node node;
    long node_offset;
    // Check free list for a valid entry
    node_offset = free_head.next;
    while (node_offset)
    {
        if (!read_node(node_offset, &node))
            return false;
        if (node.bytes >= num_bytes)
        {   // Found an appropriate block on the free list.
            if (node.bytes < num_bytes + list_node_size + minimum_size)
            {   // not worth splitting, return as is:
                if (!remove_node(reserved_space+list_node_size, &free_head,
                                 node_offset, &node) ||
                    !insert_node(reserved_space, &allocated_head,
                                 node_offset, &node))
                    return 0;
                return node_offset + list_node_size;
            }
            else
            {
                // Split requested size off, correct free list
                node.bytes -= list_node_size + num_bytes;
                free_head.bytes -= list_node_size + num_bytes;
                if (!(write_node(node_offset, &node) &&
                      write_node(reserved_space+list_node_size, &free_head)))
                    return 0;
                // Create, insert & return new node
                long new_offset = node_offset + list_node_size + node.bytes;
                list_node new_node;
                new_node.bytes = num_bytes;
                if (!insert_node(reserved_space, &allocated_head,
                                 new_offset, &new_node))
                    return 0;
                return new_offset + list_node_size;
            }
        }
        node_offset = node.next;
    }
    // Need to append new block.
    // grow file
    fseek(f, file_size+list_node_size+num_bytes-1, SEEK_SET);
    fwrite("", 1, 1, f);
    // write new node
    node.bytes = num_bytes;
    node.prev = allocated_head.prev;
    node.next = 0;
    if (!write_node(file_size, &node))
        return false;
    // maybe update what used to be the last block
    if (allocated_head.prev)
    {
        if (!read_node(allocated_head.prev, &node))
            return false;
        node.next = file_size;
        if (!write_node(allocated_head.prev, &node))
            return false;
    }
    // update head of list
    allocated_head.bytes += num_bytes;
    if (allocated_head.next == 0)
        allocated_head.next = file_size;
    allocated_head.prev = file_size;
    if (!write_node(reserved_space, &allocated_head))
        return false;
    // Update overall file size, return offset of new data block
    long data_offset = file_size + list_node_size;
    file_size = data_offset + num_bytes;
    return data_offset;
}

bool file_allocator::free(long block_offset)
{
    // list_node should precede the memory block,
    // so it cannot start before reserved_space + heads + 1st buffer node
    if (block_offset < reserved_space + 3*list_node_size  ||
        block_offset >= file_size)
    {
        fprintf(stderr,
                "file_allocator::free called with impossible offset %ld\n",
                block_offset);
        return false;
    }

    list_node node;
    long node_offset = block_offset - list_node_size;
    if (!read_node(node_offset, &node))
        return false;
    if (node_offset + node.bytes > file_size)
    {
        fprintf(stderr,
                "file_allocator::free called with broken node %ld\n",
                block_offset);
        return false;
    }
    
    // remove node at 'offset' from list of allocated blocks,
    // add node to list of free blocks
    if (! (remove_node(reserved_space, &allocated_head,
                       node_offset, &node) &&
           insert_node(reserved_space+list_node_size, &free_head,
                       node_offset, &node))
        )
        return false;
    // Check if we can merge with the preceeding block
    list_node pred;
    long pred_offset = node.prev;
    if (pred_offset)
    {
        if (!read_node(pred_offset, &pred))
            return false;
        if (pred_offset + list_node_size + pred.bytes == node_offset)
        {   // Combine this free'ed node with prev. block
            pred.bytes += list_node_size + node.bytes;
            // skip the current node
            pred.next = node.next;
            if (!write_node(pred_offset, &pred))
                return false;
            if (pred.next)
            {   // correct back pointer
                if (!read_node(pred.next, &node))
                    return false;
                node.prev = pred_offset;
                if (!write_node(pred.next, &node))
                    return false;
            }
            else
            {   // we changed the tail of the free list
                free_head.prev = pred_offset;
            }
            free_head.bytes += list_node_size;
            if (!write_node(reserved_space+list_node_size, &free_head))
                return false;
            
            // 'pred' is now the 'current' node
            node_offset = pred_offset;
            node = pred;
        }
    }

    // Check if we can merge with the succeeding block
    list_node succ;
    long succ_offset = node.next;
    if (succ_offset)
    {
        if (node_offset + list_node_size + node.bytes == succ_offset)
        {
            // Combine this free'ed node with following block
            if (!read_node(succ_offset, &succ))
                return false;
            node.bytes += list_node_size + succ.bytes;
            // skip the next node
            node.next = succ.next;
            if (!write_node(node_offset, & node))
                return false;
            if (node.next)
            {   // correct back pointer
                list_node after;
                if (!read_node(node.next, &after))
                    return false;
                after.prev = node_offset;
                if (!write_node(node.next, &after))
                    return false;
            }
            else
            {   // we changed the tail of the free list
                free_head.prev = node_offset;
            }
            free_head.bytes += list_node_size;
            return write_node(reserved_space+list_node_size, &free_head);
        }
    }
    return true;
}

void file_allocator::dump(int level)
{
    list_node allocated_node, free_node;
    long allocated_offset, free_offset;
    long allocated_prev = 0, free_prev = 0;
    long allocated_mem = 0, allocated_blocks = 0;
    long free_mem = 0, free_blocks = 0;
    long next_offset = 0;

    if (level > 0)
        printf("bytes in file: %ld. Reserved/Allocated/Free: %ld/%ld/%ld\n",
               file_size,
               reserved_space, allocated_head.bytes, free_head.bytes);
    allocated_offset = allocated_head.next;
    free_offset = free_head.next;
    while(allocated_offset || free_offset)
    {
        // Show the node that's next in the file
        if (allocated_offset &&
            (free_offset == 0 || free_offset > allocated_offset))
        {
            read_node(allocated_offset, &allocated_node);
            ++allocated_blocks;
            allocated_mem += allocated_node.bytes;
            if (level > 0)
                printf("Allocated Block @ %10ld: %ld bytes\n",
                       allocated_offset, allocated_node.bytes);
            if (next_offset && next_offset != allocated_offset)
                printf("! There is a gap, %ld unmaintained bytes before this block!\n",
                       allocated_offset - next_offset);
            if (allocated_prev != allocated_node.prev)
                printf("! Block's ''prev'' pointer is broken!\n");
            next_offset = allocated_offset + list_node_size + allocated_node.bytes;
            allocated_prev = allocated_offset;
            allocated_offset = allocated_node.next;
        }
        else if (free_offset)
        {
            read_node(free_offset, &free_node);
            ++free_blocks;
            free_mem += free_node.bytes;
            if (level > 0)
                printf("Free      Block @ %10ld: %ld bytes\n",
                       free_offset, free_node.bytes);
            if (next_offset && next_offset != free_offset)
                printf("! There is a gap, %ld unmaintained bytes before this block!\n",
                       free_offset - next_offset);
            if (free_prev != free_node.prev)
                printf("! Block's ''prev'' pointer is broken!\n");
            next_offset = free_offset + list_node_size + free_node.bytes;
            free_prev = free_offset;
            free_offset = free_node.next;
        }
    }
    if (file_size !=
        (reserved_space +
         2*list_node_size + // allocated/free list headers
         allocated_blocks*list_node_size +
         allocated_head.bytes +
         free_blocks*list_node_size +
         free_head.bytes))
    {
        printf("! The total file size does not compute!\n");
    }
    if (allocated_mem != allocated_head.bytes)
    {
        printf("! The amount of allocated space does not compute!\n");
    }
    if (free_mem != free_head.bytes)
    {
        printf("! The amount of allocated space does not compute!\n");
    }
}

bool file_allocator::read_node(long offset, list_node *node)
{
    fseek(f, offset, SEEK_SET);
    return readLong(f, &node->bytes) &&
        readLong(f, &node->prev) &&
        readLong(f, &node->next);
}

bool file_allocator::write_node(long offset, const list_node *node)
{
    fseek(f, offset, SEEK_SET);
    return writeLong(f, node->bytes) &&
        writeLong(f, node->prev) &&
        writeLong(f, node->next);
}

bool file_allocator::remove_node(long head_offset, list_node *head,
                                 long node_offset, const list_node *node)
{
    list_node tmp;
    long tmp_offset;

    if (head->next == node_offset)
    {   // first node; make head skip it
        head->bytes -= node->bytes;
        head->next = node->next;
        if (head->next == 0) // list now empty?
            head->prev = 0;
        else
        {   // correct 'prev' ptr of what's now the first node
            if (!read_node(head->next, &tmp))
                return false;
            tmp.prev = 0;
            if (!write_node(head->next, &tmp))
                return false;
        }
        return write_node(head_offset, head);
    }
    else
    {   // locate a node that's not the first
        tmp_offset = head->next;
        if (!read_node(tmp_offset, &tmp))
            return false;
        while (tmp.next != node_offset)
        {
            tmp_offset = tmp.next;
            if (!tmp_offset)
                return false;
            if (!read_node(tmp_offset, &tmp))
                return false;
        }
        // tmp/tmp_offset == node before the one to be removed
        tmp.next = node->next;
        if (!write_node(tmp_offset, &tmp))
            return false;
        if (node->next)
        {   // adjust prev pointer of node->next
            if (!read_node(node->next, &tmp))
                return false;
            tmp.prev = tmp_offset;
            if (!write_node(node->next, &tmp))
                return false;
        }
        head->bytes -= node->bytes;
        if (head->prev == node_offset)
            head->prev = node->prev;
        return write_node(head_offset, head);
    }
}

bool file_allocator::insert_node(long head_offset, list_node *head,
                                 long node_offset, list_node *node)
{
    // add node to list of free blocks
    if (head->next == 0)
    {   // first in free list
        head->next = node_offset;
        head->prev = node_offset;
        node->next = 0;
        node->prev = 0;
        head->bytes += node->bytes;
        return
            write_node(node_offset, node) &&
            write_node(head_offset, head);
    }
    // insert into free list, sorted by position (for nicer dump() printout)
    if (node_offset < head->next)
    {   // new first item
        node->prev = 0;
        node->next = head->next;
        if (!write_node(node_offset, node))
            return false;
        list_node tmp;
        if (!read_node(node->next, &tmp))
            return false;
        tmp.prev = node_offset;
        if (!write_node(node->next, &tmp))
            return false;
        head->next = node_offset;
        head->bytes += node->bytes;
        return write_node(head_offset, head);
    }
    // find proper location in free list
    list_node pred;
    long pred_offset = head->next;
    if (!read_node(pred_offset, &pred))
        return false;
    while (pred.next && pred.next < node_offset)
    {
        pred_offset = pred.next;
        if (!read_node(pred_offset, &pred))
            return false;
    }
    // pred.next == 0  or >= node_offset: insert here!
    node->next = pred.next;
    node->prev = pred_offset;
    if (!write_node(node_offset, node))
        return false;
    if (pred.next)
    {
        list_node after;
        if (!read_node(pred.next, &after))
            return false;
        after.prev = node_offset;
        if (!write_node(pred.next, &after))
            return false;
    }
    pred.next = node_offset;
    if (!write_node(pred_offset, &pred))
        return false;
    if (head->prev == pred_offset)
        head->prev = node_offset;
    head->bytes += node->bytes;
    return write_node(head_offset, head);
}
