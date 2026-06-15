
#include <iostream>
#include <cstdlib>

class TreeNode {
public:
    TreeNode * left;
    TreeNode * right;

    TreeNode(TreeNode * l = nullptr, TreeNode * r = nullptr) 
    : 
      left(l), 
      right(r) 
    {}

    ~TreeNode() {
        if (left != nullptr) {
            delete left;
        }
        if (right != nullptr) {
            delete right;
        }
    }

    int nodeCount() const {
        int result = 1;
        if (left != nullptr) {
            result += left->nodeCount();
        }
        if (right != nullptr) {
            result += right->nodeCount();
        }
        return result;
    }
};

TreeNode * buildTree(int depth) {
    if (depth == 0) {
        return new TreeNode();
    }
    return new TreeNode(buildTree(depth - 1), buildTree(depth - 1));
}

int countNodes(int depth) {
    TreeNode * t = buildTree(depth);
    int c = t->nodeCount();
    delete t;
    return c;
}

void stretch(int depth) {
    std::cout << "stretch tree of depth " << depth << "\t check: " << countNodes(depth) << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "C++\n";
    int n = 10;
    if (argc > 1) {
        n = std::atoi(argv[1]);
    }
    int minDepth = 4;
    int maxDepth = (minDepth + 2 > n) ? minDepth + 2 : n;
    int stretchDepth = maxDepth + 1;
    stretch(stretchDepth);   
    TreeNode * longLived = buildTree(maxDepth);
    for (int depth = minDepth; depth <= maxDepth; depth += 2) {
        int iterations = 1 << (maxDepth - depth + minDepth);
        int sum = 0;
        for (int i = 0; i < iterations; i++) {
            sum += countNodes(depth);
        }
        std::cout << iterations << "\t trees of depth " << depth << "\t check: " << sum << "\n";
    }
    std::cout << "long lived tree of depth " << maxDepth << "\t check: " << longLived->nodeCount() << "\n";
    delete longLived;    
    return 0;
}
