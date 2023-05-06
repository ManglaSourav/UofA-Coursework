// a02.js
// CSC544 Assignment 02
// Sourav Mangla <souravmangla@arizona.edu>
// 
// This file contain code for all the requirement of Assignment 02. 


//////////////////////////////////////////////////////////////////////////////
// Global variables, preliminaries to draw the grid

//Comment out one of the following two lines to select a dataset
// let data = iris;
let data = scores;

// Defining the size of the svg
let width = 1100;
let height = 1100;
let padding = 40;

// Creating svg canvas
let svg = d3.select("body").append("svg")
  .attr("width", width)
  .attr("height", height);

if (data && data[0].species) {

  let iris_legend = d3
    .select('body')
    .append('svg')
    .attr('width', 150)
    .attr('height', window.innerWidth > 1270 ? height - 50 : 150)

  let color = d3
    .scaleOrdinal()
    .domain(['setosa', 'versicolor', 'virginica'])
    .range(["purple", "pink", "orange"])

  iris_legend
    .append('text')
    .attr('x', 35)
    .attr('y', 135)
    .style('font-weight', 'bold')
    .text('Species')

  let legend = iris_legend
    .selectAll('.legend')
    .data(color.domain())
    .enter()
    .append('g')
    .attr('transform', function (d, i) {
      return 'translate(0, ' + (10 + i * 30) + ')';
    })

  // show color on the legend
  legend
    .append('circle')
    .attr('cx', 32)
    .attr('cy', 30)
    .attr('r', 10)
    .style('fill', color)

  // show text on the legend
  legend
    .append('text')
    .attr('x', 50)
    .attr('y', 35)
    .style('text-anchor', 'start')
    .text(function (d) {
      return d
    })
}

// Extracting the list of attributes from this data
let attribs = Object.keys(data[0]).filter(d => typeof data[0][d] === "number");

let size = (width / attribs.length);
//create the groups and do a data join to create the grid of scatterplots       
let groups = svg.selectAll("g")
  .data(d3.cross(attribs, attribs))
  .join("g")
  .attr("transform", d => `translate(${attribs.indexOf(d[0]) * size},${attribs.indexOf(d[1]) * size})`);
let attrib_pairs = d3.cross(attribs, attribs);

// Making the scatterplots for each attribute pair
groups.each(function (attrib_pair) {
  makeScatterplot(d3.select(this), attrib_pair);
})



// Adding the axis to the scatterplots
groups.each(function (attrib_pair) {
  let x = d3.scaleLinear()
    .domain(d3.extent(data, d => d[attrib_pair[0]]))
    .range([0 + padding, size - padding]);

  let y = d3.scaleLinear()
    .domain(d3.extent(data, d => d[attrib_pair[1]]))
    .range([size - padding, 0 + padding]);

  let xAxis = d3.axisBottom(x).ticks(6);
  let yAxis = d3.axisLeft(y).ticks(6);

  d3.select(this).append("g")
    .attr("transform", `translate(0,${size - padding})`)
    .call(xAxis);
  d3.select(this).append("g")
    .attr("transform", `translate(${padding},0)`)
    .call(yAxis);

  d3.select(this).append("text")
    .attr("x", size / 2)
    .attr("y", size - 5)
    .attr("text-anchor", "middle")
    .text(attrib_pair[0]);
  d3.select(this).append("text")
    .attr("x", -size / 2)
    .attr("y", -padding / 2 + 30)
    .attr("text-anchor", "middle")
    .attr("transform", "rotate(-90)")
    .text(attrib_pair[1]);
})

//////////////////////////////////////////////////////////////////////////////
// Function to make the scatteplots
function makeScatterplot(selection, attrib_pair) {

  let x = d3.scaleLinear()
    .domain(d3.extent(data, d => d[attrib_pair[0]]))
    .range([0 + padding, size - padding]);

  let y = d3.scaleLinear()
    .domain(d3.extent(data, d => d[attrib_pair[1]]))
    .range([size - padding, 0 + padding]);


  let brush = d3.brush()
    .extent([[0, 0], [size, size]])
    .on("start", updateBrush())
    .on("brush", updateBrush())
    .on("end", updateBrush());

  selection.append("g")
    .attr("class", "brush")
    .call(brush);


  let getColor
  if (data && data[0].species) {
    getColor = d3.scaleOrdinal()
      .domain(["setosa", "versicolor", "virginica"])
      .range(["purple", "pink", "orange"]);
  } else {
    let [dMin, dMax] = d3.extent(data, d => d.GPA)
    getColor = d3
      .scaleLinear()
      .domain([dMin, dMax])
      .range(["purple", "orange"])
  }

  selection.selectAll("circle")
    .data(data)
    .enter()
    .append("circle")
    .attr("cx", d => x(d[attrib_pair[0]]))
    .attr("cy", d => y(d[attrib_pair[1]]))
    .attr("r", 2.5)
    .attr("fill", d => getColor(d.species ? d.species : d.GPA))
}

// Dictionary to store the all brush selections
let my_dict = {};
// Boolean to check is there any active brush selection or not
let reset = false

//////////////////////////////////////////////////////////////////////////////
// Function to for the brush interactions
function updateBrush() {
  return function (event, d) {

    if (event.selection != null) {
      // adding the brush selection range to my dictionary (This also handle the update operationa as well)
      my_dict[d[0] + "_" + d[1]] = event.selection;
      reset = false;
    }
    else {
      // removing the brush selection range from my dictionary
      delete my_dict[d[0] + "_" + d[1]];
    }
    // check if the dictionary is empty or not (We need to reset the color)
    if (Object.keys(my_dict).length === 0) {
      reset = true;
    }
    onBrush();
  };
}

function onBrush() {
  if (reset) {
    // reset the all circle color to the original color

    let getColor
    if (data && data[0].species) {
      getColor = d3
        .scaleOrdinal()
        .domain(['setosa', 'versicolor', 'virginica'])
        .range(["purple", "pink", "orange"]);
    } else {
      let [dMin, dMax] = d3.extent(data, d => d.GPA)
      getColor = d3
        .scaleLinear()
        .domain([dMin, dMax])
        .range(["purple", "orange"])
    }

    d3.select("body")
      .selectAll("circle")
      .attr("fill", d => getColor(d.species ? d.species : d.GPA))
      .attr("opacity", 1);

  } else {

    function isSelected(d) {
      let count = 0;
      // looping through the dictionary and checking if the data point is in the brush selection
      for (let key in my_dict) {
        let pair = key.split("_");

        let x = d3.scaleLinear()
          .domain(d3.extent(data, d => d[pair[0]]))
          .range([0 + padding, size - padding]);
        let y = d3.scaleLinear()
          .domain(d3.extent(data, d => d[pair[1]]))
          .range([size - padding, 0 + padding]);

        let x0 = x(d[pair[0]]) >= my_dict[key][0][0];
        let y0 = y(d[pair[1]]) >= my_dict[key][0][1];
        let x1 = x(d[pair[0]]) <= my_dict[key][1][0];
        let y1 = y(d[pair[1]]) <= my_dict[key][1][1];

        if (x0 && x1 && y0 && y1)
          count++;
      }

      // if the circle is in all the selected ranges, return true
      if (count == Object.keys(my_dict).length) {
        return true;
      }
    }

    let allCircles = d3.select("body").selectAll("circle");

    // update the style for the selected circles
    allCircles
      .filter(isSelected)
      .attr("stroke", "black")
      .attr("opacity", 1);

    // update the style for the unselected circles
    allCircles
      .filter(d => !isSelected(d))
      .attr("stroke", "none")
      .attr("opacity", 0.5);
  }
}

