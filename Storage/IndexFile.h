/// -*- c++ -*-

#ifndef __INDEX_FILE_H__
#define __INDEX_FILE_H__

// Storage
#include <Index.h>
#include <NameHash.h>
#include <RTree.h>

/// \ingroup Storage
/// @{
/// Defines routines for the RTree-based index.

/// The archiver's index file.

/// The IndexFile combines the NameHash for channel name
/// lookup with one RTree per channel into an index.
///
/// The file itself starts with the IndexFile cookie,
/// followed by the NameHash anchor.
/// Those two items constitute the 'reserved space'
/// all the remaining space is handled by the FileAllocator.
/// The ID of each NameHash entry points to an RTree anchor.
class IndexFile : public Index
{
public:
    // == 'CAI2', Chan. Arch. Index 2
    static const uint32_t cookie = 0x43414932;

    IndexFile(int RTreeM);

    /// The hash table size used for new channel name tables.
    static uint32_t ht_size;
    
    /// Open an index.
    bool open(const stdString &filename, bool readonly=true);

    /// Close the index.
    void close();
    
    /// Add a channel to the index.

    /// A channel has to be added before data blocks get defined
    /// for the channel. When channel is already in index, existing
    /// tree gets returned.
    ///
    /// Caller must delete the tree pointer.
    class RTree *addChannel(const stdString &channel,
                            stdString &directory);

    /// Obtain the RTree for a channel.

    /// Caller not delete the tree pointer.
    ///
    ///
    class RTree *getTree(const stdString &channel, stdString &directory);

    /// Locate NameIterator on first channel.
    bool getFirstChannel(NameIterator &iter);

    /// Locate NameIterator on next channel.

    /// \pre Successfull call to get_first_channel().
    bool getNextChannel(NameIterator &iter);

    void showStats(FILE *f);   

    bool check(int level);
    
private:
    int RTreeM;
    FILE *f;
    FileAllocator fa;
    NameHash names;
    stdString dirname;
};

/// @}

#endif
