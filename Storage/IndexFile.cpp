// Tools
#include <AutoPtr.h>
#include <BinIO.h>
#include <MsgLogger.h>
#include <Filename.h>
// Index
#include "IndexFile.h"
#include "RTree.h"

// File Layout:
// long cookie;
// NameHash::anchor_size anchor;
// Rest handled by FileAllocator, NameHash and RTree

uint32_t IndexFile::ht_size = 1009;

IndexFile::IndexFile(int RTreeM) : RTreeM(RTreeM), f(0), names(fa, 4)
{}

IndexFile::~IndexFile()
{
    close();
}

void IndexFile::open(const stdString &filename, bool readonly)
{
    stdString linked_filename;
    if (Filename::getLinkedFilename(filename, linked_filename))
        Filename::getDirname(linked_filename, dirname);
    else
        Filename::getDirname(filename, dirname);
    bool new_file = false;
    if (readonly)
        f.open(filename.c_str(), "rb");
    else
    {   // Try existing file
        f.open(filename.c_str(), "r+b");
        if (!f)
        {   // Create new file
            f.open(filename.c_str(), "w+b");
            new_file = true;
        }   
    }
    if (!f)
        throw GenericException(__FILE__, __LINE__,
                               "IndexFile::open(%s) cannot %s file.",
                               filename.c_str(), (new_file ? "create" : "open"));
    // TODO: Tune these two. All 0 seems best?!
    FileAllocator::minimum_size = 0;
    FileAllocator::file_size_increment = 0;
    fa.attach(f, 4+NameHash::anchor_size);
    if (new_file)
    {
        if (fseek(f, 0, SEEK_SET))
            throw GenericException(__FILE__, __LINE__,
                                   "IndexFile::open(%s) seek error",
                                   filename.c_str());
        if (!writeLong(f, cookie))
            throw GenericException(__FILE__, __LINE__,
                                   "IndexFile::open(%s) cannot write cookie.",
                                   filename.c_str());
        names.init(ht_size);
        return; // OK, created new file.
    }
    // Check existing file
    uint32_t file_cookie;
    if (fseek(f, 0, SEEK_SET))
        throw GenericException(__FILE__, __LINE__,
                               "IndexFile::open(%s) seek error",     
                               filename.c_str());
    if (!readLong(f, &file_cookie))
        throw GenericException(__FILE__, __LINE__,
                               "IndexFile::open(%s) cannot read cookie.",
                               filename.c_str());
    if (file_cookie != cookie)
        throw GenericException(__FILE__, __LINE__,
                               "IndexFile::open(%s) Invalid cookie, "
                               "0x%08lX instead of 0x%08lX.",
                               filename.c_str(),
                               (unsigned long)file_cookie,
                               (unsigned long)cookie);
    names.reattach();
}

void IndexFile::close()
{
    if (f)
    {
        fa.detach();
        f.close();
    }   
}

RTree *IndexFile::addChannel(const stdString &channel, stdString &directory)
{   // Using AutoPtr in case e.g. tree->init throws exception.
    AutoPtr<RTree> tree(getTree(channel, directory));
    if (tree)
        return tree.release(); // Done, found existing.
    FileOffset tree_anchor = fa.allocate(RTree::anchor_size);
    try
    {
        tree = new RTree(fa, tree_anchor);
    }
    catch (...)
    {
        tree.release();
        throw GenericException(__FILE__, __LINE__,
                               "Cannot allocate RTree for channel '%s'",
                               channel.c_str());
    }
    stdString tree_filename;
    tree->init(RTreeM);
    if (names.insert(channel, tree_filename, tree_anchor) == false)
        throw GenericException(__FILE__, __LINE__,
                               "Index consistency error: New channel '%s' "
                               "had no RTree but was already in name hash.",
                               channel.c_str());
    // Done, new name entry & RTree.
    directory = dirname;
    return tree.release();
}

RTree *IndexFile::getTree(const stdString &channel, stdString &directory)
{
    stdString  tree_filename;
    FileOffset tree_anchor;
    if (!names.find(channel, tree_filename, tree_anchor))
        return 0; // All OK, but channel not found.
    // Using AutoPtr in case e.g. tree->reattach throws exception.
    AutoPtr<RTree> tree;
    try
    {
        tree = new RTree(fa, tree_anchor);
    }
    catch (...)
    {
        tree.release();
        throw GenericException(__FILE__, __LINE__,
                               "Cannot allocate RTree for channel '%s'",
                               channel.c_str());
    }
    tree->reattach();
    directory = dirname;
    return tree.release();
}

bool IndexFile::getFirstChannel(NameIterator &iter)
{
    return names.startIteration(iter.hashvalue, iter.entry);
}

bool IndexFile::getNextChannel(NameIterator &iter)
{
    return names.nextIteration(iter.hashvalue, iter.entry);
}

void IndexFile::showStats(FILE *f)
{
    names.showStats(f);
}

bool IndexFile::check(int level)
{
    printf("Checking FileAllocator...\n");
    try
    {
        if (fa.dump(level))
            printf("FileAllocator OK\n");
        else
        {
            printf("FileAllocator ERROR\n");
            return false;
        }
        NameIterator names;
        bool have_name;
        unsigned long channels = 0;
        unsigned long total_nodes=0, total_used_records=0, total_records=0;
        unsigned long nodes, records;
        stdString dir;
        for (have_name = getFirstChannel(names);
             have_name;
             have_name = getNextChannel(names))
        {
            ++channels;
            AutoPtr<RTree> tree(getTree(names.getName(), dir));
            if (!tree)
            {
                printf("%s not found\n", names.getName().c_str());
                return false;
            }
            printf(".");
            fflush(stdout);
            if (!tree->selfTest(nodes, records))
            {
                printf("RTree for channel '%s' is broken\n",
                       names.getName().c_str());
                return false;
            }
            total_nodes += nodes;
            total_used_records += records;
            total_records += nodes * tree->getM();
        }
        printf("\nAll RTree self-tests check out fine\n");
        printf("%ld channels\n", channels);
        printf("Total: %ld nodes, %ld records out of %ld are used (%.1lf %%)\n",
               total_nodes, total_used_records, total_records,
               total_used_records*100.0/total_records);
    }
    catch (GenericException &e)
    {
        printf("Exception:\n%s\n", e.what());
    }
    return true;
}

