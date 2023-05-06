//
// a06.js
// CSC544 Assignment 06
// Sourav Mangla <souravmangla@arizona.edu>
//
// This implements vector field visualization techniques and relies on
// flowvis.js to perform the data loading of vector fields in VTK's .vti
// format.
//
// It expects a a div with id 'vfplot' to where the vector field will be
// visualized
//

////////////////////////////////////////////////////////////////////////
// Global variables and helper functions


//this variable will hold the vector field upon loading
let vf = null;

//variables for the svg canvas
let svg = null;
let height = 800;
let width = 600;

////////////////////////////////////////////////////////////////////////

// Visual Encoding portion that handles the d3 aspects
var xScale = null;
var yScale = null;
var colorScale = null;
var mScale = null;
let fileName = null;

// Function to create the d3 objects
function initializeSVG() {
    //Since we will call this function multiple times, we'll clear out the
    //svg if it exists
    if (svg != null) {
        svg.remove()
    }
    //vf.bounds will report the units of the vector field
    //use aspect ratio to update width/height
    let aspectRatio = (vf.bounds[3] - vf.bounds[2]) / (vf.bounds[1] - vf.bounds[0])
    let [v0, v1] = vf.range
    let [b0, b1, b2, b3] = vf.bounds
    width = height / aspectRatio;
    //Initialize the SVG canvas
    svg = d3.select("#vfplot")
        .append("svg")
        .attr("width", width + 15)
        .attr("height", height);

    //TODO: Create scales for x, y, color and magnitude
    colorScale = d3.scaleLinear()
        .domain([v0, v1])
        .range(["blue", "red"]);

    // magnitude scale
    mScale = d3.scaleLinear()
        .domain([v0, v1])
        .range([10, 60]);

    xScale = d3.scaleLinear()
        .domain([b0, b1])
        .range([20, width]);

    yScale = d3.scaleLinear()
        .domain([b3, b2])
        .range([20, height - 20]);


    //vf.range will report the minimum/maximum magnitude
    let xAxis = d3.axisBottom(xScale).ticks(10);
    let yAxis = d3.axisLeft(yScale).ticks(10);

    svg.append("g")
        .attr("transform", "translate(0," + (height - 20) + ")")
        .call(xAxis);

    svg.append("g")
        .attr("transform", `translate(20,0)`)
        .call(yAxis);

}

function loadRandomSeed() {
    randomSeedPosition = [];
    let [b0, b1, b2, b3] = vf.bounds;
    for (let i = 0; i < 1200; i++) {
        let x = Math.random() * (b1 - b0) + b0;
        let y = Math.random() * (b3 - b2) + b2;
        if (x < b0 || y < b2 || x > b1 || y > b3) continue;
        else randomSeedPosition.push([x, y]);
    }
    return randomSeedPosition;
}

function loadUniformSeed() {
    uniformSeedPosition = [];
    let [b0, b1, b2, b3] = vf.bounds;
    for (let i = 0; i < 2000; i = i + 12)
        for (let j = 0; j < 1500; j = j + 15) {
            let x = b0 + i * (b1 - b0) / 600;
            let y = b2 + j * (b3 - b2) / 500;
            if (x < b0 || y <= b2 || x >= b1 || y >= b3) continue;
            else uniformSeedPosition.push([x, y]);
        }
    return uniformSeedPosition;
}

function rk4Integrate([s0, s1]) {
    let vector = vf.interpolate(s0, s1)
    let [vx, vy] = vector
    let k1x = dt * vx
    let k1y = dt * vy

    vector = vf.interpolate(s0 + k1x / 2, s1 + k1y / 2)
    [vx, vy] = vector
    let k2x = dt * vx
    let k2y = dt * vy

    vector = vf.interpolate(s0 + k2x / 2, s1 + k2y / 2)
    [vx, vy] = vector
    let k3x = dt * vx
    let k3y = dt * vy

    vector = vf.interpolate(s0 + k3x, s1 + k3y)
    [vx, vy] = vector
    let k4x = dt * vx
    let k4y = dt * vy

    let x = s0 + (1 / 6) * (k1x + 2 * k2x + 2 * k3x + k4x)
    let y = s1 + (1 / 6) * (k1y + 2 * k2y + 2 * k3y + k4y)
    return [x, y]
}

function drawStreamlines(seedPositions) {
    dtDict = {
        '3cylflow.vti': 5,
        'bickley_jet.vti': 5,
        'cosine.vti': 0.01,
        'focus.vti': 0.01,
        'italy.vti': 0.01,
        'stuart_vortex.vti': 0.01,
    }
    dt = dtDict[fileName];
    const numSteps = 80;
    const path = new Array(numSteps);
    for (let i = 0; i < seedPositions.length; i++) {
        path[0] = seedPositions[i];
        for (let j = 1; j < numSteps; ++j) {
            path[j] = rk4Integrate(path[j - 1])
        }
        svg.append("path")
            .datum(path)
            .attr("d",
                d3.line()
                    .x(d => xScale(d[0]))
                    .y(d => yScale(d[1]))
            )
            .attr("stroke", (d) => {
                let [v0, v1] = vf.interpolate(path[0][0], path[0][1]);
                return colorScale(Math.sqrt(v0 * v0 + v1 * v1));
            })
            .attr("fill", "none")

        svg.append("circle")
            .data(path)
            .attr("cx", xScale(path[0][0]))
            .attr("cy", yScale(path[0][1]))
            .attr("r", 1.3)
            .attr("fill", 'black')
    }
}

function drawGlyphs(seedPositions) {
    svg.selectAll("path")
        .attr('id', 'glyphs')
        .data(seedPositions)
        .enter()
        .append("path")
        .attr("d", function (d) {
            let [v0, v1] = vf.interpolate(d[0], d[1]);
            let magnitude = Math.sqrt(v0 * v0 + v1 * v1);
            return ("M  0, 4, 0, -4," + mScale(magnitude) + " ," + "0" + "Z")
        })
        .attr("transform", function (d) {
            let [v0, v1] = vf.interpolate(d[0], d[1]);
            return "translate(" + xScale(d[0]) + "," + yScale(d[1]) + ") rotate(" + Math.atan2(-v1, v0) * 180 / Math.PI + ")";
        })
        .attr("fill", function (d) {
            let [v0, v1] = vf.interpolate(d[0], d[1]);
            let magnitude = Math.sqrt(v0 * v0 + v1 * v1);
            return colorScale(magnitude);
        })
        .attr("opacity", 0.5)
}

////////////////////////////////////////////////////////////////////////
// Function to read data
// Function to process the upload
function upload() {
    if (input.files.length > 0) {
        let file = input.files[0];
        console.log("You chose", file.name);
        fileName = file.name;
        let fReader = new FileReader();
        fReader.readAsArrayBuffer(file);

        fReader.onload = function (e) {
            let fileData = fReader.result;

            //load the .vti data and initialize volren
            vf = parseVTKFile(fileData);

            initializeSVG();
            seedPositions = null
            if (d3.select('#pos').property('value') == "Uniform")
                seedPositions = loadUniformSeed()
            else
                seedPositions = loadRandomSeed()

            if (d3.select('#vector').property('value') == "Glyphs")
                drawGlyphs(seedPositions)
            else
                drawStreamlines(seedPositions)

        }
    }
}

// Attach upload process to the loadData button
var input = document.getElementById("loadData");
input.addEventListener("change", upload);

////////////////////////////////////////////////////////////////////////
// Functions to respond to selections
d3.select('#pos').on('change', () => {
    initializeSVG();
    seedPositions = null
    if (d3.select('#pos').property('value') == "Uniform")
        seedPositions = loadUniformSeed()
    else
        seedPositions = loadRandomSeed()

    if (d3.select('#vector').property('value') == "Glyphs")
        drawGlyphs(seedPositions)
    else
        drawStreamlines(seedPositions)
});

d3.select('#vector').on('change', () => {
    initializeSVG();
    seedPositions = null
    if (d3.select('#pos').property('value') == "Uniform")
        seedPositions = loadUniformSeed()
    else
        seedPositions = loadRandomSeed()

    if (d3.select('#vector').property('value') == "Glyphs")
        drawGlyphs(seedPositions)
    else
        drawStreamlines(seedPositions)
});