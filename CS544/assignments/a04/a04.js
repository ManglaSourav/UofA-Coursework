// 
// a04.js
// CSC544 Assignment 04
// Sourav Mangla <souravmangla>@arizona.edu
//
// This file provides the code for the treemap visualization.
//

////////////////////////////////////////////////////////////////////////
// Global variables for the dataset 

// HINT: Start with one of the smaller test datesets included in
// test-cases.js instead of the larger tree in flare.js
//let data = test_1;
//let data = test_2;
let data = flare;
let winWidth = window.innerWidth;
let winHeight = window.innerHeight;
let colorScale = d3.scaleOrdinal()
  .domain([0, 1, 2, 3])
  .range(['#7A0177', '#C51B8A', '#F768A1', '#FBB4B9']);
////////////////////////////////////////////////////////////////////////
// Tree related helper functions

function setTreeSize(tree) {
  if (tree.children !== undefined) {
    let size = 0;
    for (let i = 0; i < tree.children.length; ++i) {
      size += setTreeSize(tree.children[i]);
    }
    tree.size = size;
  }
  if (tree.children === undefined) {
    // do nothing, tree.size is already defined for leaves
  }
  return tree.size;
};

function setTreeCount(tree) {
  if (tree.children !== undefined) {
    let count = 0;
    for (let i = 0; i < tree.children.length; ++i) {
      count += setTreeCount(tree.children[i]);
    }
    tree.count = count;
  }
  if (tree.children === undefined) {
    tree.count = 1;
  }
  return tree.count;
}

function setTreeDepth(tree, depth) {
  // sets the count of the current subtree in the tree.
  let maxDepth = depth;
  // Current tree is not a leaf node
  if (tree.children !== undefined) {
    for (let i = 0; i < tree.children.length; ++i) {
      let currDepth = setTreeDepth(tree.children[i], depth + 1);
      if (currDepth > maxDepth) {
        maxDepth = currDepth;
      }
    }
    tree.depth = depth;
  }
  // Current tree is a leaf node
  if (tree.children === undefined) {
    tree.depth = depth;
  }
  return maxDepth;
};


// Initialize the size, count, and depth variables within the tree
setTreeSize(data);
setTreeCount(data);
let maxDepth = setTreeDepth(data, 0);


////////////////////////////////////////////////////////////////////////
// Main Code for the Treemapping Technique
function setRectangles(rect, tree, attrFun) {
  tree.rect = rect;
  let vertically;
  let flag
  if (winWidth > winHeight) {
    flag = 0;
  } else {
    flag = 1;
  }
  if (tree.depth % 2 === flag) {
    vertically = true;
  } else {
    vertically = false;
  }
  if (tree.children !== undefined) {
    let cumulativeSizes = [0];
    for (let i = 0; i < tree.children.length; ++i) {
      cumulativeSizes.push(cumulativeSizes[i] + attrFun(tree.children[i]));
    }

    let rectWidth = rect.x2 - rect.x1;
    let rectHeight = rect.y2 - rect.y1;
    let border = 5;

    // TODO: WRITE THIS PART.
    // Hint: set the range of the "scale" variable above appropriately,
    // depending on the shape of the current rectangle and the splitting
    // direction.  This will help you define newRect for each child.

    for (let i = 0; i < tree.children.length; ++i) {
      let x1, x2, y1, y2;
      // Layout current tree node's children based on splitting orientation
      if (vertically) {
        if (i === 0) {
          x1 = rect.x1 + border;
        }
        else {
          x1 = tree.children[i - 1].rect.x2;
        }
        x2 = x1 + (attrFun(tree.children[i]) / attrFun(tree)) * (rectWidth - border * 2);
        y1 = rect.y1 + border;
        y2 = rect.y2 - border;
      } else {
        x1 = rect.x1 + border;
        x2 = rect.x2 - border;
        if (i === 0) {
          y1 = rect.y1 + border;
        }
        else {
          y1 = tree.children[i - 1].rect.y2;
        }
        y2 = y1 + (attrFun(tree.children[i]) / attrFun(tree)) * (rectHeight - border * 2);
      }
      let newRect = { x1: x1, x2: x2, y1: y1, y2: y2 };
      setRectangles(newRect, tree.children[i], attrFun);
    }
  }
}

function best(rect, tree, attrFun) {
  tree.rect = rect;
  let flag = (winWidth > winHeight ? 0 : 1);
  let vertically;
  let border = 5;
  if (tree.children !== undefined) {
    let cumulativeSizes = [0];
    for (let i = 0; i < tree.children.length; ++i) {
      cumulativeSizes.push(cumulativeSizes[i] + attrFun(tree.children[i]));
    }

    let rectWidth = rect.x2 - rect.x1;
    let rectHeight = rect.y2 - rect.y1;

    let scale = d3.scaleLinear()
      .domain([0, cumulativeSizes[cumulativeSizes.length - 1]])
      .range([0, vertically ? rectWidth : rectHeight]);
    for (let i = 0; i < tree.children.length; ++i) {
      let x1, x2, y1, y2;
      if (tree.children[i].children === undefined) {
        if (rectHeight <= rectWidth) {
          vertically = true;
        }
        else {
          vertically = false;
        }
      }
      else {
        if (tree.depth % 2 === flag) {
          vertically = true;
        } else {
          vertically = false;
        }
      }
      if (vertically) {
        if (i === 0) {
          x1 = rect.x1 + border;
        }
        else {
          x1 = tree.children[i - 1].rect.x2;
        }
        x2 = x1 + (attrFun(tree.children[i]) / attrFun(tree)) * (rectWidth - border * 2);
        y1 = rect.y1 + border;
        y2 = rect.y2 - border;
      } else {
        x1 = rect.x1 + border;
        x2 = rect.x2 - border;
        if (i === 0) {
          y1 = rect.y1 + border;
        }
        else {
          y1 = tree.children[i - 1].rect.y2;
        }
        y2 = y1 + (attrFun(tree.children[i]) / attrFun(tree)) * (rectHeight - border * 2);
      }
      let newRect = { x1: x1, x2: x2, y1: y1, y2: y2 };
      best(newRect, tree.children[i], attrFun);
    }
  }
}

best({ x1: 0, y1: 0, x2: winWidth, y2: winHeight }, data, node => node.size);

// compute the rectangles for each tree node
setRectangles(
  { x1: 0, y1: 0, x2: winWidth, y2: winHeight }, data,
  function (t) { return t.size; }
);

// make a list of all tree nodes;
function makeTreeNodeList(tree, lst) {
  lst.push(tree);
  if (tree.children !== undefined) {
    for (let i = 0; i < tree.children.length; ++i) {
      makeTreeNodeList(tree.children[i], lst);
    }
  }
}

let treeNodeList = [];
makeTreeNodeList(data, treeNodeList);



////////////////////////////////////////////////////////////////////////
// Visual Encoding portion

// d3 selection to draw the tree map 
let gs = d3.select("#svg")
  .attr("width", winWidth)
  .attr("height", winHeight)
  .selectAll("g")
  .data(treeNodeList)
  .enter()
  .append("g");

function setAttrs(sel) {
  sel.attr("width", function (treeNode) {
    let widthDiff = treeNode.rect.x2 - treeNode.rect.x1;
    if (widthDiff < 0) {
      return 0;
    }
    else {
      return widthDiff;
    }
  })
    .attr("height", function (treeNode) {
      return treeNode.rect.y2 - treeNode.rect.y1;
    })
    .attr("x", function (treeNode) {
      return treeNode.rect.x1;
    })
    .attr("y", function (treeNode) {
      return treeNode.rect.y1;
    })
    .attr("fill", function (treeNode) {
      if (treeNode.depth === 0) {
        return "#FEEBE2";
      }
      else {
        return colorScale(treeNode.depth);
      }
    })
    .attr("stroke", function (treeNode) {
      return "black";
    })
    .attr('title', function (treeNode) {
      return treeNode.name;
    })
}

gs.append("rect").call(setAttrs);

////////////////////////////////////////////////////////////////////////
// Callbacks for buttons

d3.select("#size").on("click", function () {
  setRectangles(
    { x1: 0, x2: winWidth, y1: 0, y2: winHeight }, data,
    function (t) { return t.size; }
  );
  d3.selectAll("rect").transition().duration(1000).call(setAttrs);
});

d3.select("#count").on("click", function () {
  setRectangles(
    { x1: 0, x2: winWidth, y1: 0, y2: winHeight }, data,
    function (t) { return t.count; }
  );
  d3.selectAll("rect").transition().duration(1000).call(setAttrs);
});

d3.select("#best-size").on("click", function () {
  best(
    { x1: 0, x2: winWidth, y1: 0, y2: winHeight }, data,
    function (t) { return t.size; }
  );
  d3.selectAll("rect").transition().duration(1000).call(setAttrs);
});
d3.select("#best-count").on("click", function () {
  best(
    { x1: 0, x2: winWidth, y1: 0, y2: winHeight }, data,
    function (t) { return t.count; }
  );
  d3.selectAll("rect").transition().duration(1000).call(setAttrs);
});