#include <iostream>
#include <boost/filesystem.hpp>
#include <cryfs/config/CryConfigFile.h>
#include <cryfs/config/CryPasswordBasedKeyProvider.h>
#include <blockstore/implementations/ondisk/OnDiskBlockStore2.h>
#include <blockstore/implementations/low2highlevel/LowToHighLevelBlockStore.h>
#include <blobstore/implementations/onblocks/datanodestore/DataNodeStore.h>
#include <blobstore/implementations/onblocks/datanodestore/DataNode.h>
#include <blobstore/implementations/onblocks/datanodestore/DataInnerNode.h>
#include <blobstore/implementations/onblocks/datanodestore/DataLeafNode.h>
#include <blobstore/implementations/onblocks/BlobStoreOnBlocks.h>
#include <cryfs/filesystem/fsblobstore/FsBlobStore.h>
#include <cryfs/filesystem/fsblobstore/DirBlob.h>
#include <cryfs/filesystem/CryDevice.h>
#include <cpp-utils/io/IOStreamConsole.h>

#include <set>

using namespace boost;
using namespace boost::filesystem;
using namespace std;
using namespace cryfs;
using namespace cpputils;
using namespace blockstore;
using namespace blockstore::ondisk;
using namespace blockstore::lowtohighlevel;
using namespace blobstore::onblocks;
using namespace blobstore::onblocks::datanodestore;
using namespace cryfs::fsblobstore;

void printNode(unique_ref<DataNode> node) {
    std::cout << "BlockId: " << node->blockId().ToString() << ", Depth: " << node->depth() << " ";
    auto innerNode = dynamic_pointer_move<DataInnerNode>(node);
    if (innerNode != none) {
        std::cout << "Type: inner\n";
        return;
    }
    auto leafNode = dynamic_pointer_move<DataLeafNode>(node);
    if (leafNode != none) {
        std::cout << "Type: leaf\n";
        return;
    }
}

set<BlockId> _getBlockstoreUnaccountedBlocks(const CryConfig &config) {
    auto onDiskBlockStore = make_unique_ref<OnDiskBlockStore2>("/home/heinzi/basedir");
    auto encryptedBlockStore = CryCiphers::find(config.Cipher()).createEncryptedBlockstore(std::move(onDiskBlockStore), config.EncryptionKey());
    auto highLevelBlockStore = make_unique_ref<LowToHighLevelBlockStore>(std::move(encryptedBlockStore));
    auto nodeStore = make_unique_ref<DataNodeStore>(std::move(highLevelBlockStore), config.BlocksizeBytes());
    std::set<BlockId> unaccountedBlocks;
    uint32_t numBlocks = nodeStore->numNodes();
    uint32_t i = 0;
    cout << "There are " << nodeStore->numNodes() << " blocks." << std::endl;
    // Add all blocks to unaccountedBlocks
    for (auto file = directory_iterator("/home/heinzi/basedir"); file != directory_iterator(); ++file) {
        cout << "\r" << (++i) << "/" << numBlocks << flush;
        if (file->path().filename() != "cryfs.config") {
            auto blockId = BlockId::FromString(file->path().filename().string().c_str());
            unaccountedBlocks.insert(blockId);
        }
    }
    i = 0;
    cout << "\nRemove blocks that have a parent" << endl;
    //Remove root block from unaccountedBlocks
    unaccountedBlocks.erase(BlockId::FromString(config.RootBlob()));
    //Remove all blocks that have a parent node from unaccountedBlocks
    for (auto file = directory_iterator("/home/heinzi/basedir"); file != directory_iterator(); ++file) {
        cout << "\r" << (++i) << "/" << numBlocks << flush;
        if (file->path().filename() != "cryfs.config") {
            auto blockId = BlockId::FromString(file->path().filename().string().c_str());
            auto node = nodeStore->load(blockId);
            auto innerNode = dynamic_pointer_move<DataInnerNode>(*node);
            if (innerNode != none) {
                for (uint32_t childIndex = 0; childIndex < (*innerNode)->numChildren(); ++childIndex) {
                    auto child = (*innerNode)->readChild(childIndex).blockId();
                    unaccountedBlocks.erase(child);
                }
            }
        }
    }
    return unaccountedBlocks;
}

set<BlockId> _getBlocksReferencedByDirEntries(const CryConfig &config) {
    auto onDiskBlockStore = make_unique_ref<OnDiskBlockStore2>("/home/heinzi/basedir");
    auto encryptedBlockStore = CryCiphers::find(config.Cipher()).createEncryptedBlockstore(std::move(onDiskBlockStore), config.EncryptionKey());
    auto highLevelBlockStore = make_unique_ref<LowToHighLevelBlockStore>(std::move(encryptedBlockStore));
    auto fsBlobStore = make_unique_ref<FsBlobStore>(make_unique_ref<BlobStoreOnBlocks>(std::move(highLevelBlockStore), config.BlocksizeBytes()));
    set<BlockId> blocksReferencedByDirEntries;
    uint32_t numBlocks = fsBlobStore->numBlocks();
    uint32_t i = 0;
    cout << "\nRemove blocks referenced by dir entries" << endl;
    for (auto file = directory_iterator("/home/heinzi/basedir"); file != directory_iterator(); ++file) {
        cout << "\r" << (++i) << "/" << numBlocks << flush;
        if (file->path().filename() != "cryfs.config") {
            auto blockId = BlockId::FromString(file->path().filename().string().c_str());
            try {
                auto blob = fsBlobStore->load(blockId);
                if (blob != none) {
                    auto dir = dynamic_pointer_move<DirBlob>(*blob);
                    if (dir != none) {
                        vector<fspp::Dir::Entry> children;
                        (*dir)->AppendChildrenTo(&children);
                        for (const auto &child : children) {
                            blocksReferencedByDirEntries.insert((*dir)->GetChild(child.name)->blockId());
                        }
                    }
                }
            } catch (...) {}
        }
    }
    return blocksReferencedByDirEntries;
}


int main() {
    auto console = std::make_shared<cpputils::IOStreamConsole>();

    console->print("Loading config\n");
    auto askPassword = [console] () {
        return console->askPassword("Password: ");
    };
    auto keyProvider = make_unique_ref<CryPasswordBasedKeyProvider>(
        console,
        askPassword,
        askPassword,
        make_unique_ref<SCrypt>(SCrypt::DefaultSettings)
    );
    auto config = CryConfigFile::load("/home/heinzi/basedir/cryfs.config", keyProvider.get());
    set<BlockId> unaccountedBlocks = _getBlockstoreUnaccountedBlocks(*config->config());
    //Remove all blocks that are referenced by a directory entry from unaccountedBlocks
    set<BlockId> blocksReferencedByDirEntries = _getBlocksReferencedByDirEntries(*config->config());
    for (const auto &blockId : blocksReferencedByDirEntries) {
        unaccountedBlocks.erase(blockId);
    }

    console->print("Calculate statistics\n");

    auto onDiskBlockStore = make_unique_ref<OnDiskBlockStore2>("/home/heinzi/basedir");
    auto encryptedBlockStore = CryCiphers::find(config->config()->Cipher()).createEncryptedBlockstore(std::move(onDiskBlockStore), config->config()->EncryptionKey());
    auto highLevelBlockStore = make_unique_ref<LowToHighLevelBlockStore>(std::move(encryptedBlockStore));
    auto nodeStore = make_unique_ref<DataNodeStore>(std::move(highLevelBlockStore), config->config()->BlocksizeBytes());

    uint32_t numUnaccountedBlocks = unaccountedBlocks.size();
    uint32_t numLeaves = 0;
    uint32_t numInner = 0;
    console->print("Unaccounted blocks: " + std::to_string(unaccountedBlocks.size()) + "\n");
    for (const auto &blockId : unaccountedBlocks) {
        console->print("\r" + std::to_string(numLeaves+numInner) + "/" + std::to_string(numUnaccountedBlocks));
        auto node = nodeStore->load(blockId);
        auto innerNode = dynamic_pointer_move<DataInnerNode>(*node);
        if (innerNode != none) {
            ++numInner;
            printNode(std::move(*innerNode));
        }
        auto leafNode = dynamic_pointer_move<DataLeafNode>(*node);
        if (leafNode != none) {
            ++numLeaves;
            printNode(std::move(*leafNode));
        }
    }
    console->print("\n" + std::to_string(numLeaves) + " leaves and " + std::to_string(numInner) + " inner nodes\n");
}
