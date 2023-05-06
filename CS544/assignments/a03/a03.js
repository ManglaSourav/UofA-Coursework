// 
// a03.js
// CSC544 Assignment 03
// Sourav Mangla <souravmangla@arizona.edu>
//
// This file provides code for all the functionality required for parallel coordinates plot  
//


////////////////////////////////////////////////////////////////////////
// Global variables for the dataset 
let data = cars;

// dims will store the seven numeric axes in left-to-right display order
let dims = [
  "economy (mpg)",
  "cylinders",
  "displacement (cc)",
  "power (hp)",
  "weight (lb)",
  "0-60 mph (s)",
  "year"
];

////////////////////////////////////////////////////////////////////////
// Global variables for the svg canvas
let width = dims.length * 125;
let height = 500;
let padding = 50;

let svg = d3.select("#pcplot")
  .append("svg")
  .attr("width", width).attr("height", height);


////////////////////////////////////////////////////////////////////////
// Initialize the x and y scales, axes, and brushes.  
//  - xScale stores a mapping from dimension id to x position
//  - yScales[] stores each y scale, one per dimension id
//  - axes[] stores each axis, one per id
//  - brushes[] stores each brush, one per id
//  - brushRanges[] stores each brush's event.selection, one per id

let xScale = d3.scalePoint()
  .domain(dims)
  .range([padding, width - padding]);

let yScales = {};
let colorScales = {};
let axes = {};
let brushes = {};
let brushRanges = {};

defaultColor = "economy (mpg)";

function setColorScale(d) {
  let [min, max] = d3.extent(data, function (datum) {
    return datum[d];
  });
  color = []
  if (d == "economy (mpg)") {
    color = ["Blue", "yellow"]
  } else if (d == "cylinders") {
    color = ["red", "green"]
  } else if (d == "displacement (cc)") {
    color = ["Purple", "orange"]
  } else if (d == "power (hp)") {
    color = ["Brown", "#fde725ff"]
  } else if (d == "weight (lb)") {
    color = ["#21908dff", "#440154ff"]
  } else if (d == "0-60 mph (s)") {
    color = ["Gold", "red"]
  } else if (d == "year") {
    color = ["pink", "black"];
  }
  return d3.scaleSequential()
    .domain([min, max])
    .interpolator(d3.interpolate(color[0], color[1]));
}


// For each dimension, we will initialize a yScale, axis, brush, and
// brushRange
dims.forEach(function (dim) {
  //create a scale for each dimension
  yScales[dim] = d3.scaleLinear()
    .domain(d3.extent(data, function (datum) { return datum[dim]; }))
    .range([height - padding, padding]);

  //created a color scale for each dimension
  let [min, max] = d3.extent(data, function (datum) {
    return datum[dim];
  });
  // setting color scale for each dimension
  colorScales[dim] = setColorScale(dim)

  //set up a vertical axis for each dimensions
  axes[dim] = d3.axisLeft()
    .scale(yScales[dim])
    .ticks(10)


  //set up brushes as a 20 pixel width band
  //we will use transforms to place them in the right location
  brushes[dim] = d3.brushY()
    .extent([[-10, padding], [10, height - padding]])


  //brushes will be hooked up to their respective updateBrush functions
  brushes[dim]
    .on("brush", updateBrush(dim))
    .on("end", updateBrush(dim))

  //initial brush ranges to null
  brushRanges[dim] = null;
});



////////////////////////////////////////////////////////////////////////
// Make the parallel coordinates plots 
// add the actual polylines for data elements, each with class "datapath"
function path(d) {
  return d3.line()(dims.map(function (d_Name) {
    return [xScale(d_Name), yScales[d_Name](d[d_Name])];
  }));
}

svg.append("g")
  .selectAll(".datapath")
  .data(data)
  .enter()
  .append("path")
  .attr("class", "datapath")
  .attr("d", (d) => path(d))
  .attr("fill", "none")
  .attr("opacity", 0.75)
  .attr("stroke", (d) => colorScales[defaultColor](d[defaultColor]))

// add the axis groups, each with class "axis"
svg.selectAll(".axis")
  .data(dims)
  .enter()
  .append("g")
  .classed("axis", true)
  .each(function (d) { d3.select(this).call(d3.axisLeft().scale(yScales[d])) })
  .attr("transform", (d) => "translate(" + xScale(d) + ",0)")

svg.selectAll(".label")
  .data(dims)
  .enter()
  .append("text")
  .classed("label", true)
  .attr('transform', d => 'translate(' + xScale(d) + ')')
  .attr('x', -30)
  .attr('y', 30)
  .text(d => d)
  .on("click", onClick)
  .on("mouseover", onMouseOver)

// add the brush groups, each with class ".brush" 
svg.selectAll(".brush")
  .data(dims)
  .enter()
  .append("g")
  .classed("brush", true)
  .each(function (d) { d3.select(this).call(brushes[d]) })
  .attr("transform", function (d) { return "translate(" + xScale(d) + ",0)" })
  .selectAll("rect")
  .attr("height", height - 2 * padding);


////////////////////////////////////////////////////////////////////////
// Interaction Callbacks

// Callback for swaping axes when a text label is clicked.
function onClick(event, dim) {

  // Swap the dimensions in the dims array.
  let index1 = dims.indexOf(dim);
  let index2;
  if (index1 == dims.length - 1) {
    index2 = index1 - 1;
  } else {
    index2 = index1 + 1;
  }
  let temp = dims[index1];
  dims[index1] = dims[index2];
  dims[index2] = temp;

  // Update the xScales, axes, data lines and labels.
  xScale = d3.scalePoint()
    .domain(dims)
    .range([padding, width - padding]);

  d3.selectAll(".label")
    .transition()
    .duration(1000)
    .attr('x', -30)
    .attr('y', 30)
    .text((d) => d)
    .attr("font-weight", "normal") // reset all labels to normal because onMouseOver and onClick happen at the same time
    .attr("transform", (d) => "translate(" + xScale(d) + ")")

  d3.selectAll(".datapath")
    .transition()
    .duration(1000)
    .attr("d", path)
    .attr("opacity", 0.75)
    .attr("stroke", (d) => colorScales[defaultColor](d[defaultColor]))

  d3.selectAll(".axis")
    .transition()
    .duration(1000)
    .attr("transform", (d) => "translate(" + xScale(d) + ")");

}

function onMouseOver(event, dim) {

  // bolding the label  and changing the opacity and color of the dimension that is being hovered over
  d3.selectAll(".datapath")
    .transition()
    .duration(1000)
    .attr("stroke", (d) => colorScales[dim](d[dim]))

  d3
    .selectAll(".label")
    .attr("font-weight", (d) => d == dim ? "bold" : "normal")
}

// Returns a callback function that calls onBrush() for the brush
// associated with each dimension
function updateBrush(dim) {
  return function (event) {
    brushRanges[dim] = event.selection;
    onBrush();
  };
}

// Callback when brushing to select elements in the PC plot
function onBrush() {
  let allLines = d3.selectAll(".datapath");

  function isSelected(d) {
    let selected = true;
    for (let dim in brushRanges) {
      if (brushRanges[dim] != null) {
        let [min, max] = brushRanges[dim];
        if (yScales[dim](d[dim]) < min || yScales[dim](d[dim]) > max)
          selected = false;
      }
    }
    return selected;
  }


  //Update the style of the selected and not selected data
  allLines
    .filter(isSelected)
    .attr("opacity", 0.75);

  allLines
    .filter(function (d) { return !isSelected(d); })
    .attr("opacity", 0.1);

}