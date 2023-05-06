// 
// a05.js
// CSC544 Assignment 05
// Sourav Mangla <souravmangla>@arizona.edu
//
// This implements an editable transfer function to be used in concert
// with the volume renderer defined in volren.js
//
// It expects a div with id 'tfunc' to place the d3 transfer function
// editor
//


////////////////////////////////////////////////////////////////////////
// Global variables and helper functions

// colorTF and opacityTF store a list of transfer function control
// points.  Each element should be [k, val] where k is a the scalar
// position and val is either a d3.rgb or opacity in [0,1] 
let colorTF = [];
let opacityTF = [];

// D3 layout variables
let size = 500;
let svg = null;


// Scale Variables 
let xScale = null;
let yScale = null;
let colorScale = null;

////////////////////////////////////////////////////////////////////////
// Visual Encoding portion that handles the d3 aspects

// Function to create the d3 objects
function initializeTFunc() {
  svg = d3.select("#tfunc")
    .append("svg")
    .attr("width", size)
    .attr("height", size);

  //Initializing the axes and color scale
  colorScale = d3
    .scaleSequential(d3.interpolateCool)
    .domain([dataRange[0], dataRange[1]]);
  xScale = d3
    .scaleLinear()
    .domain([dataRange[0], dataRange[1]])
    .range([70, 450]);
  yScale = d3
    .scaleLinear()
    .domain([0, 1])
    .range([450, 70]);
  let xAxis = d3
    .axisBottom()
    .scale(xScale)
    .ticks(10);
  let yAxis = d3
    .axisLeft()
    .scale(yScale)
    .ticks(10);

  // setting the x-axis 
  svg.append("g")
    .attr("transform", `translate(0,${xScale(1)})`)
    .attr("id", "xline")
    .call(xAxis)

  // setting the y-axis 
  svg.append("g")
    .attr("transform", `translate(${yScale(1)},0)`)
    .attr("id", "yline")
    .call(yAxis)

  //Initializing opacity TF curve and connecting each circle 
  svg.append("g")
    .attr("class", "pathLine")
    .selectAll("path")
    .data(opacityTF)
    .enter()
    .append("path")
    .attr("d", function (d, i) {
      if (i != 0)
        return "M " + xScale(opacityTF[i - 1][0]) + " " +
          yScale(opacityTF[i - 1][1]) + " " +
          xScale(d[0]) + " " + yScale(d[1]);
    })
    .attr("stroke", "black");


  //Initialize circles controls
  let drag = d3.drag()
    .on('start', dragInitiated)
    .on('drag', dragged)
    .on('end', dragEnded);

  svg.append("g")
    .attr("class", "points")
    .selectAll("circle")
    .data(opacityTF)
    .enter()
    .append("circle")
    .attr("index", (d, i) => i)
    .style('cursor', 'pointer')
    .attr("cx", d => xScale(d[0]))
    .attr("cy", d => yScale(d[1]))
    .attr("r", 7)
    .attr("fill", d => colorScale(d[0] / dataRange[1]))
    .call(drag)

  //Create the color bar to show the color TF
  lineScale = d3.scaleLinear().domain([0, 50]).range([70, 450]);

  //array used to draw the color bar.
  a = [];
  for (i = 0; i <= 50; i++)
    a.push(i);

  svg.append("g")
    .attr("class", "rects")
    .selectAll("rect")
    .data(a)
    .enter()
    .append("rect")
    .attr("x", function (d) { return lineScale(d); })
    .attr("y", 470)
    .attr("width", 10)
    .attr("height", 30)
    .attr("fill", function (d) {
      return colorScale(d * dataRange[1] / 50);
    })

  //After initializing, set up anything that depends on the TF arrays
  updateTFunc();
}

// Call this function whenever a new dataset is loaded or whenever
// colorTF and opacityTF change
function updateTFunc() {
  //updating the scales
  xScale = d3.scaleLinear().domain([dataRange[0], dataRange[1]]).range([70, 450]);
  yScale = d3.scaleLinear().domain([0, 1]).range([450, 70]);

  //default color scale used sequential
  if (colorScale == null) {
    colorScale = d3.scaleSequential(d3.interpolateCool).domain([dataRange[0], dataRange[1]]);
  }

  //connecting axes to updated scales
  let x = d3.axisBottom().scale(xScale).ticks(10);
  let y = d3.axisLeft().scale(yScale).ticks(10);
  svg
    .select("#xline")
    .call(x)
  svg
    .select("#yline")
    .call(y)

  //updating opacity curves
  d3.select(".points")
    .selectAll("circle")
    .data(opacityTF)
    .attr("cx", d => xScale(d[0]))
    .attr("cy", d => yScale(d[1]))
    .attr("r", 7)
    .attr("fill", function (d) {
      return colorScale(d[0]);
    })
  d3.select(".pathLine")
    .selectAll("path")
    .data(opacityTF)
    .attr("d", function (d, i) {
      if (i != 0)
        return "M " + xScale(opacityTF[i - 1][0]) + " " + yScale(opacityTF[i - 1][1]) + " "
          + xScale(d[0]) + " " + yScale(d[1]);
    })

  //updating color bar
  svg.select(".rects")
    .selectAll("rect")
    .data(a)
    .attr("fill", function (d) {
      return colorScale(d * dataRange[1] / 50);
    })
}


// To start, let's reset the TFs and then initialize the d3 SVG canvas
// to draw the default transfer function
resetTFs();
initializeTFunc();


////////////////////////////////////////////////////////////////////////
// Interaction callbacks

//Methods needed for selection, dragging, etc.
// Will track which point is selected
let selected = null;

// Called when mouse down
function dragInitiated(event, d) {
  selected = parseInt(d3.select(this).attr("index"));
}

function dragged(event, d) {
  if (selected == null)
    return
  let pos = [];
  pos[0] = xScale.invert(event.x);
  pos[1] = yScale.invert(event.y);

  let left = selected == 0 ? dataRange[0] : opacityTF[selected - 1][0];
  let right = selected == opacityTF.length - 1 ? dataRange[1] : opacityTF[selected + 1][0];
  // corner points only allowed to move in y-axis
  left = selected == opacityTF.length - 1 ? dataRange[1] : left;
  right = selected == 0 ? dataRange[0] : right;
  pos[0] = Math.max(left, Math.min(right, pos[0]));
  pos[1] = Math.max(0, Math.min(1, pos[1]));

  //based on pos and selected, update opacityTF
  opacityTF[selected] = pos;

  //update TF window and volume renderer
  updateTFunc();
  updateVR(colorTF, opacityTF);

}

function dragEnded() {
  selected = null;
}

////////////////////////////////////////////////////////////////////////
// Function to read data

// Function to process the upload
function upload() {
  if (input.files.length > 0) {
    let file = input.files[0];
    console.log("You chose", file.name);

    let fReader = new FileReader();
    fReader.readAsArrayBuffer(file);

    fReader.onload = function (e) {
      let fileData = fReader.result;

      //load the .vti data and initialize volren
      initializeVR(fileData);

      //upon load, we'll reset the transfer functions completely
      resetTFs();

      //Update the tfunc canvas
      updateTFunc();

      //update the TFs with the volren
      updateVR(colorTF, opacityTF, false);
    }
  }
}

// Attach upload process to the loadData button
var input = document.getElementById("loadData");
input.addEventListener("change", upload);


////////////////////////////////////////////////////////////////////////
// Functions to respond to buttons that switch color TFs
function resetTFs() {
  makeSequential();
  makeOpacity();
}

// Make a default opacity TF
function makeOpacity() {
  opacityTF = [
    [dataRange[0], 0.5],
    [dataRange[1] / 6, 0.3],
    [dataRange[1] * 2 / 6, 1],
    [dataRange[1] * 3 / 6, 1],
    [dataRange[1] * 4 / 6, 0.1],
    [dataRange[1] * 5 / 6, 1],
    [dataRange[1] * 6 / 6, 1],
  ];
}

// Make default sequential color TF and handling other scales as well
function makeSequential() {
  if (colorScale == null) {
    colorScale = d3.scaleSequential(d3.interpolateCool).domain([dataRange[0], dataRange[1]]);
  }
  colorScale.domain([dataRange[0], dataRange[1]]);
  colorTF = [
    [dataRange[0], d3.rgb(colorScale(0))],
    [dataRange[1] / 6, d3.rgb(colorScale(dataRange[1] / 6))],
    [dataRange[1] * 2 / 6, d3.rgb(colorScale(dataRange[1] * 2 / 6))],
    [dataRange[1] * 3 / 6, d3.rgb(colorScale(dataRange[1] * 3 / 6))],
    [dataRange[1] * 4 / 6, d3.rgb(colorScale(dataRange[1] * 4 / 6))],
    [dataRange[1] * 5 / 6, d3.rgb(colorScale(dataRange[1] * 5 / 6))],
    [dataRange[1] * 6 / 6, d3.rgb(colorScale(dataRange[1]))]
  ];
}


// Configure callbacks for each button
d3.select("#sequential").on("click", function () {
  colorScale = d3.scaleSequential(d3.interpolateCool).domain([dataRange[0], dataRange[1]]);
  makeSequential();
  updateTFunc();
  updateVR(colorTF, opacityTF, true);
});
d3.select("#diverging").on("click", function () {
  colorScale = d3.scaleSequential(d3.interpolateRdBu).domain([dataRange[0], dataRange[1]]);
  makeSequential();
  updateTFunc();
  updateVR(colorTF, opacityTF, true);
});
d3.select("#categorical").on("click", function () {
  colorScale = d3.scaleQuantize(d3.schemePaired).domain([dataRange[0], dataRange[1]]);
  makeSequential();
  updateTFunc();
  updateVR(colorTF, opacityTF, true);
});


