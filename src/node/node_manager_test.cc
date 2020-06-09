/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * node_manager_test.cc
 *
 * Author: chenjing
 * Date: 2019/10/28
 *--------------------------------------------------------------------------
 **/

#include "node/node_manager.h"
#include <glog/logging.h>
#include "gtest/gtest.h"

namespace fesql {
namespace node {

class NodeManagerTest : public ::testing::Test {
 public:
    NodeManagerTest() {}

    ~NodeManagerTest() {}
};

TEST_F(NodeManagerTest, MakeSQLNode) {
    NodeManager *manager = new NodeManager();
    manager->MakeTableNode("", "table1");
    manager->MakeTableNode("", "table2");
    manager->MakeLimitNode(10);

    manager->MakeTablePlanNode("t1");
    manager->MakeTablePlanNode("t2");
    manager->MakeTablePlanNode("t3");

    ASSERT_EQ(3, manager->GetParserNodeListSize());
    ASSERT_EQ(3, manager->GetPlanNodeListSize());
    delete manager;
}
TEST_F(NodeManagerTest, MakeAndExprTest) {
    NodeManager *manager = new NodeManager();
    manager->MakeTableNode("", "table1");
    manager->MakeTableNode("", "table2");
    manager->MakeLimitNode(10);

    ExprListNode expr_list;
    expr_list.AddChild(manager->MakeBinaryExprNode(
        manager->MakeColumnRefNode("col1", "t1"),
        manager->MakeColumnRefNode("col1", "t2"), node::kFnOpEq));
    expr_list.AddChild(manager->MakeBinaryExprNode(
        manager->MakeColumnRefNode("col2", "t1"),
        manager->MakeColumnRefNode("col2", "t2"), node::kFnOpEq));
    expr_list.AddChild(manager->MakeBinaryExprNode(
        manager->MakeColumnRefNode("col3", "t1"),
        manager->MakeColumnRefNode("col3", "t2"), node::kFnOpEq));

    ASSERT_EQ("t1.col1 = t2.col1 AND t1.col2 = t2.col2 AND t1.col3 = t2.col3",
              node::ExprString(manager->MakeAndExpr(&expr_list)));
    delete manager;
}

TEST_F(NodeManagerTest, MergeFrameNode_RowsTest) {
    NodeManager manager;
    // [-100, 0] U [-150, 0] = [150, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 100),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 120),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRows, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_rows()->start()->bound_type());
        ASSERT_EQ(-120, merged->frame_rows()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_rows()->end()->bound_type());
    }

    // [-100, 0] U [-150, -50] = [150, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 100),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 120),
                                    manager.MakeFrameBound(kPreceding, 50))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRows, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_rows()->start()->bound_type());
        ASSERT_EQ(-120, merged->frame_rows()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_rows()->end()->bound_type());
    }

    // [-100, 50] U [-30, 0] = [100, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 100),
                                    manager.MakeFrameBound(kPreceding, 50))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 30),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRows, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_rows()->start()->bound_type());
        ASSERT_EQ(-100, merged->frame_rows()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_rows()->end()->bound_type());
    }

    // [-100, 50] U [-30, 80] = [100, 80]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 100),
                                    manager.MakeFrameBound(kPreceding, 50))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 30),
                                    manager.MakeFrameBound(kFollowing, 80))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRows, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_rows()->start()->bound_type());
        ASSERT_EQ(-100, merged->frame_rows()->start()->GetSignedOffset());
        ASSERT_EQ(kFollowing, merged->frame_rows()->end()->bound_type());
        ASSERT_EQ(80, merged->frame_rows()->end()->GetSignedOffset());
    }
}
TEST_F(NodeManagerTest, MergeFrameNode_RangeTest) {
    NodeManager manager;
    // [-1d, 0] U [-6h, 0] = [1d, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRange, manager.MakeFrameExtent(
                             manager.MakeFrameBound(
                                 kPreceding, manager.MakeConstNode(1, kDay)),
                             manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRange, manager.MakeFrameExtent(
                             manager.MakeFrameBound(
                                 kPreceding, manager.MakeConstNode(6, kHour)),
                             manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }

    // [-1d, 0] U [-6h, -30m] = [1d, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRange, manager.MakeFrameExtent(
                             manager.MakeFrameBound(
                                 kPreceding, manager.MakeConstNode(1, kDay)),
                             manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(6, kHour)),
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(30, kMinute)))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }

    // [-1d, -30] U [-6h, -0] = [1d, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(30, kMinute)))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRange, manager.MakeFrameExtent(
                             manager.MakeFrameBound(
                                 kPreceding, manager.MakeConstNode(6, kHour)),
                             manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }
}

TEST_F(NodeManagerTest, MergeFrameNode_RowsRangeTest) {
    NodeManager manager;
    // [-1d, 0] U [-6h, 0] = [1d, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(6, kHour)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRowsRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }

    // [-1d, 0] U [-6h, -30m] = [1d, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(6, kHour)),
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(30, kMinute)))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRowsRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }

    // [-1d, -30] U [-6h, -0] = [1d, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(30, kMinute)))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(6, kHour)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRowsRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }

    // [-1d, 0] U [UNBOUND, -30m] = [UNBOUND, 0]
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPrecedingUnbound),
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(30, kMinute)))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRowsRange, merged->frame_type());
        ASSERT_EQ(kPrecedingUnbound,
                  merged->frame_range()->start()->bound_type());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
    }
}

TEST_F(NodeManagerTest, RowMergeRowsRangeTest) {
    NodeManager manager;
    // [-1d, 0] U [-1000, 0] = [-1d, 0],rows_size = 1000
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 1000),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame1, frame2);
        ASSERT_EQ(kFrameRowsRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
        ASSERT_EQ(-1000L, merged->frame_rows()->start()->GetSignedOffset());
    }

    // [-1d, 0] U [-1000, 0] U [-10000, 0] = [-1d, 0],rows_size = 10000
    {
        FrameNode *frame1 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRowsRange,
            manager.MakeFrameExtent(
                manager.MakeFrameBound(kPreceding,
                                       manager.MakeConstNode(1, kDay)),
                manager.MakeFrameBound(kCurrent))));
        FrameNode *frame2 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 1000),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *frame3 = manager.MergeFrameNode(frame1, frame2);
        FrameNode *frame4 = dynamic_cast<FrameNode *>(manager.MakeFrameNode(
            kFrameRows,
            manager.MakeFrameExtent(manager.MakeFrameBound(kPreceding, 10000),
                                    manager.MakeFrameBound(kCurrent))));
        FrameNode *merged = manager.MergeFrameNode(frame3, frame4);
        ASSERT_EQ(kFrameRowsRange, merged->frame_type());
        ASSERT_EQ(kPreceding, merged->frame_range()->start()->bound_type());
        ASSERT_EQ(-86400000, merged->frame_range()->start()->GetSignedOffset());
        ASSERT_EQ(kCurrent, merged->frame_range()->end()->bound_type());
        ASSERT_EQ(-10000L, merged->frame_rows()->start()->GetSignedOffset());
    }
}

}  // namespace node
}  // namespace fesql

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
