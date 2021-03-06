#include "testutils/DataTreeTest.h"
#include <gmock/gmock.h>

using ::testing::WithParamInterface;
using ::testing::Values;

using blobstore::onblocks::datanodestore::DataNodeLayout;
using blockstore::BlockId;

class DataTreeTest_NumStoredBytes: public DataTreeTest {
public:
};

TEST_F(DataTreeTest_NumStoredBytes, CreatedTreeIsEmpty) {
  auto tree = treeStore.createNewTree();
  EXPECT_EQ(0u, tree->numStoredBytes());
}

class DataTreeTest_NumStoredBytes_P: public DataTreeTest_NumStoredBytes, public WithParamInterface<uint32_t> {};
INSTANTIATE_TEST_CASE_P(EmptyLastLeaf, DataTreeTest_NumStoredBytes_P, Values(0u));
INSTANTIATE_TEST_CASE_P(HalfFullLastLeaf, DataTreeTest_NumStoredBytes_P, Values(5u, 10u));
INSTANTIATE_TEST_CASE_P(FullLastLeaf, DataTreeTest_NumStoredBytes_P, Values(static_cast<uint32_t>(DataNodeLayout(DataTreeTest_NumStoredBytes::BLOCKSIZE_BYTES).maxBytesPerLeaf())));

TEST_P(DataTreeTest_NumStoredBytes_P, SingleLeaf) {
  BlockId blockId = CreateLeafWithSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, TwoLeafTree) {
  BlockId blockId = CreateTwoLeafWithSecondLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(nodeStore->layout().maxBytesPerLeaf() + GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, FullTwolevelTree) {
  BlockId blockId = CreateFullTwoLevelWithLastLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(nodeStore->layout().maxBytesPerLeaf()*(nodeStore->layout().maxChildrenPerInnerNode()-1) + GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, ThreeLevelTreeWithOneChild) {
  BlockId blockId = CreateThreeLevelWithOneChildAndLastLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(nodeStore->layout().maxBytesPerLeaf() + GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, ThreeLevelTreeWithTwoChildren) {
  BlockId blockId = CreateThreeLevelWithTwoChildrenAndLastLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(nodeStore->layout().maxBytesPerLeaf()*nodeStore->layout().maxChildrenPerInnerNode() + nodeStore->layout().maxBytesPerLeaf() + GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, ThreeLevelTreeWithThreeChildren) {
  BlockId blockId = CreateThreeLevelWithThreeChildrenAndLastLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(2*nodeStore->layout().maxBytesPerLeaf()*nodeStore->layout().maxChildrenPerInnerNode() + nodeStore->layout().maxBytesPerLeaf() + GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, FullThreeLevelTree) {
  BlockId blockId = CreateFullThreeLevelWithLastLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(nodeStore->layout().maxBytesPerLeaf()*nodeStore->layout().maxChildrenPerInnerNode()*(nodeStore->layout().maxChildrenPerInnerNode()-1) + nodeStore->layout().maxBytesPerLeaf()*(nodeStore->layout().maxChildrenPerInnerNode()-1) + GetParam(), tree->numStoredBytes());
}

TEST_P(DataTreeTest_NumStoredBytes_P, FourLevelMinDataTree) {
  BlockId blockId = CreateFourLevelMinDataWithLastLeafSize(GetParam())->blockId();
  auto tree = treeStore.load(blockId).value();
  EXPECT_EQ(nodeStore->layout().maxBytesPerLeaf()*nodeStore->layout().maxChildrenPerInnerNode()*nodeStore->layout().maxChildrenPerInnerNode() + GetParam(), tree->numStoredBytes());
}
