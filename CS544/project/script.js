var data_directory = "./data1.json"
var w = 500;
var h = 500;
var padding = 40;
var spider_ratio = 0;

function makeTitle(svg, name, subtitle) {
    svg.append("text")
        .attr("transform", "translate(" + padding + "," + 25 + ")")
        .attr("dy", "0em")
        .attr("font-size", "1.2em")
        .style("text-anchor", "left")
        .style("font-family", "'Courier New', Courier, monospace")
        .text(name);

    svg.append("text")
        .attr("transform", "translate(" + padding + "," + 25 + ")")
        .attr("dy", "1.4em")
        .attr("font-size", "0.9em")
        .style("text-anchor", "left")
        .style("opacity", 0.7)
        .style("font-family", "'Courier New', Courier, monospace")
        .text(subtitle);
}

function makeAxisTitles(svg, yaxis, xaxis) {
    svg.append("text")
        .attr("transform", "translate(60)")
        .attr("dy", "1em")
        .style("font-size", "1.2em")
        .style("text-anchor", "middle")
        .style("font-family", "'Courier New', Courier, monospace")
        .text(yaxis);

    svg.append("text")
        .attr("transform", "translate(" + (w / 2) + "," + (h - padding / 2) + ")")
        .attr("dy", "1em")
        .attr("class", "x-axis-title")
        .attr("font-size", "1.2em")
        .style("text-anchor", "middle")
        .style("font-family", "'Courier New', Courier, monospace")
        .text(xaxis);
}

// Create the color function, and generate data for legends
colorlegend = [20e3, 40e3, 60e3, 80e3, 100e3]

var color = d3.scaleSequential(d3.interpolateViridis)
    .domain([0, 140000]);

function makeXscale(data, name) {

    var min = d3.min(data.features, function (d) { return d.properties[name]; })
    var max = d3.max(data.features, function (d) { return d.properties[name]; })
    var xScale = d3.scaleLinear()
        .domain([min, max])
        .range([0 + padding + 40, w - padding])
    return xScale
}

// Function to make legend for chloropleth
function makeLegend(svg, data, yoffset = (h / 2)) {
    // Create legend
    var legend = svg.selectAll("g")
        .data(data)
        .enter()
        .append("g")
        .attr("transform", function (d, i) { return "translate(" + 150 + "," + (yoffset + padding + (i * 20)) + ")" })

    legend.append("rect")
        .attr("class", "legend-rect")
        .attr("x", w - 150)
        .attr("width", 19)
        .attr("height", 19)
        .attr("fill", function (d) { return color(d) })

    legend.append("text")
        .attr("class", "legend-text")
        .attr("x", w - 100)
        .attr("font-family", "'Courier New', Courier, monospace")
        .attr("y", 9.5)
        .attr("dy", "0.4em")
        .attr("font-size", "0.8em")
        .text(function (d) { return formatAsThousands(d); })

    return legend
}

var formatAsThousands = d3.format(","); //e.g. converts 123456 to "123,456"

//Create SVG element
var svg = d3.select("body").select("#Plot1")
    .append("svg")
    .attr("width", 800)
    .attr("height", 500),

    svg2 = d3.select("body").select("#Plot2")
        .append("svg")
        .attr("width", w + 50)
        .attr("height", h),

    svg3 = d3.select("body").select("#Plot3")
        .append("svg")
        .attr('id', 'brushPlot')
        .attr("width", w)
        .attr("height", h + 40),

    svg4 = d3.select("body").select("#Plot4")
        .append("svg")
        .attr("width", w)
        .attr("height", h)

// Make legends
makeLegend(svg, colorlegend)

svg.append("text")
    .attr("transform", "translate(" + 495 + "," + 275 + ")")
    .style("font-family", "'Courier New', Courier, monospace")
    .text("INCOME (USD)")

svg.append("text")
    .classed('pollData', true)
    .attr("transform", "translate(" + (w / 2 - 150) + "," + ((h - 30)) + ")")
    .style("font-family", "'Courier New', Courier, monospace")


svg.append("text")
    .classed('stateData', true)
    .attr("transform", "translate(" + (w / 2 - 150) + "," + ((h - 50)) + ")")
    .style("font-family", "'Courier New', Courier, monospace")

makeTitle(svg, "Air Pollution Data",
    "Data form Census and EPA")
makeTitle(svg2, "Pollution attributes", "Click attribute name to filter data")

makeAxisTitles(svg3, "INCOME", "PM25")
makeAxisTitles(svg4, "POPULATION", "PM25")


d3.json(data_directory, function (err, json) {

    var INCOME_yScale = d3.scaleLinear()
        .domain([0, d3.max(json.features, function (d) { return d.properties['INCOME']; })])
        .range([h - padding, 0 + padding])

    var INCOME_yAxis = d3.axisLeft()
        .scale(INCOME_yScale)

    svg3.append("g")
        .attr("class", "y-axis")
        .style("font-family", "'Courier New', Courier, monospace")
        .style("font-size", "0.75em")
        .attr("transform", "translate(" + (padding + 40) + "," + 0 + ")")
        .call(INCOME_yAxis)

    var path = d3.geoPath()
        .projection(d3.geoAlbersUsa()
            .fitSize([w - 50, h - 50], json))

    //Bind data and create one path per GeoJSON feature
    var geos = svg.selectAll("path")
        .data(json.features)
        .enter()
        .append("path")
        .attr("d", path)
        .attr("transform", "translate(50,50)")
        .style("fill", function (d) {
            //Get data value
            var value = d.properties.INCOME;
            if (value) {
                return color(value);
            } else {
                return "#ccc";
            }
        });

    geos.on("mouseover", function (d) {
        d3.select(this)
            .transition()
            .duration(150)
            .style("fill", "orange")

        var toshow = "NO2: " + Math.ceil(formatAsThousands(d.properties.NO2))
            + " PM25: " + Math.ceil(d.properties.PM25)
            + " MaxAQI: " + Math.ceil(formatAsThousands(d.properties.MaxAQI))
            + " CO2: " + d.properties.CO
            + " Transit: " + d.properties.TRANSIT

        svg.select('text.pollData')
            .text(toshow)
            .attr("position", "absolute")


        var stateD = "County: " + d.properties.CTYNAME.toUpperCase()
            + " Population: " + Math.ceil((d.properties.POPULATION))
            + " Income: " + Math.ceil((d.properties.INCOME))

        svg.select('text.stateData')
            .text(stateD)
            .attr("position", "absolute")

        d3.select("#tooltip")
            .style("left", (d3.event.pageX) + "px")
            .style("top", (d3.event.pageY - 30) + "px")
            .select("#value")
            .text(d.properties.CTYNAME)

        d3.select("#tooltip").classed("hidden", false)

        svg3.selectAll("circle")
            .transition()
            .duration(200)
            .filter(function (l) { return d.properties.KEY == l.properties.KEY })
            .style("fill", "orange")
            .attr("r", 10)

        svg4.selectAll("circle")
            .transition()
            .duration(200)
            .filter(function (l) { return d.properties.KEY == l.properties.KEY })
            .style("fill", "orange")
            .attr("r", 10)

        var update_data = calc_data(d.properties)
        update_poly = []
        update_data.forEach(function (d, i) {
            if (d > 0) {
                d == 5
            }
            x = (radius * d) * (1 - Math.sin(i * (2 * Math.PI) / total))
            x += (h / 2 + padding * spider_ratio - (radius * d))
            y = (radius * d) * (1 - Math.cos(i * (2 * Math.PI) / total))
            y += (h / 2 + padding - (radius * d))
            update_poly.push({ 'x': x, 'y': y })
        })
        svg2.selectAll("circle")
            .data(update_data)
            .transition()
            .duration(200)
            .attr("cx", function (l, i) { return ((radius * l) * (1 - Math.sin(i * radians / total))) })
            .attr("cy", function (l, i) { return ((radius * l) * (1 - Math.cos(i * radians / total))) })
            .attr("r", 5)
            .attr("transform", function (l) { return ("translate(" + (h / 2 + padding * spider_ratio - (radius * l)) + "," + (h / 2 + padding - (radius * l)) + ")") })
            .attr("fill", "black")
            .attr("opacity", "0.8")

        svg2.selectAll("polygon")
            .data([update_poly])
            .transition()
            .duration(200)
            .style("stroke", "black")
            .style('fill', "gray")
            .attr("points", function (l) {
                return l.map(function (l) {
                    return [l.x, l.y].join(",");
                }).join(" ")
            })
            .attr("opacity", 0.5)
    })

    geos.on("mouseout", function (d) {
        d3.select(this)
            .transition()
            .duration(300)
            .style("fill", function (d) {
                //Get data value
                var value = d.properties.INCOME;
                if (value) {
                    //If value exists…
                    return color(value);
                } else {
                    //If value is undefined…
                    return "#ccc";
                }
            })
        svg.select('text.pollData').text("")
        svg.select('text.stateData').text("")

        d3.select("#tooltip").classed("hidden", true)
        svg3.selectAll("circle").style("fill", "gray").attr("r", 5)
        svg4.selectAll("circle").style("fill", "gray").attr("r", 5)
    })

    // **********************************
    // ******** SECOND PLOT**************
    // **********************************

    function calc_data(d) {
        popmax = d3.max(json.features, function (d) { return d.properties['POPULATION']; })
        INCOMEmax = d3.max(json.features, function (d) { return d.properties['INCOME']; })
        PM25max = d3.max(json.features, function (d) { return d.properties['PM25']; })
        NO2max = d3.max(json.features, function (d) { return d.properties['NO2']; })
        TRANSITmax = d3.max(json.features, function (d) { return d.properties['TRANSIT']; })
        COmax = d3.max(json.features, function (d) { return d.properties['CO']; })
        AQImean = d3.max(json.features, function (d) { return d.properties['MaxAQI']; })

        var transitS = d3.scaleLinear()
            .domain([0, 4913166])
            .range([0, 1])

        var aqiS = d3.scaleLinear()
            .domain([0, 800])
            .range([0, 6])


        data = [d.POPULATION / popmax, d.INCOME / INCOMEmax, d.NO2 / NO2max, d.PM25 / PM25max, d.CO / COmax,
        transitS(d.TRANSIT), aqiS(d.MaxAQI / 6.64)]

        return data
    }


    var sample_data = calc_data(json.features[3].properties)
    var alldata = [1, 1, 1, 1, 1, 1, 1, 1]
    var total = sample_data.length;
    var radius = h / 2 - 25
    var radians = 2 * Math.PI

    function calc_xy(value, i) {
        x = (h / 2 * value) * (1 - Math.sin(i * (2 * Math.PI) / 7))
        y = (h / 2 * value) * (1 - Math.cos(i * (2 * Math.PI) / 7))
        return [x, y]
    }

    polygonValues = []
    sample_data.forEach(function (d, i) {
        x = (radius * d) * (1 - Math.sin(i * (2 * Math.PI) / 7))
        x += (h / 2 + padding * spider_ratio - (radius * d))
        y = (radius * d) * (1 - Math.cos(i * (2 * Math.PI) / 7))
        y += (h / 2 + padding - (radius * d))
        polygonValues.push({ 'x': x, 'y': y })
    })



    var radarAxis = svg2.selectAll(".axis")
        .data(sample_data)
        .enter()
        .append("g")
        .attr("transform", "translate(" + ((w - h) / 2) + "," + (padding) + ")")
        .attr("class", "axis")

    radarAxis.append("line")
        .attr("x1", h / 2)
        .attr("y1", h / 2)
        .attr("x2", function (d, i) { return h / 2 * (1 - 0.9 * Math.sin(i * radians / total)) })
        .attr("y2", function (d, i) { return h / 2 * (1 - 0.9 * Math.cos(i * radians / total)) })
        .attr("class", "line")
        .style("stroke", "grey")

    for (var l = 0; l < 3; l++) {
        radarAxis.selectAll(".line")
            .data(alldata)
            .enter()
            .append("line")
            .attr("x1", function (d, i) { return ((radius * (l + 1) / 3) * (1 - Math.sin(i * radians / total))) })
            .attr("x2", function (d, i) { return ((radius * (l + 1) / 3) * (1 - Math.sin((i + 1) * radians / total))) })
            .attr("y1", function (d, i) { return ((radius * (l + 1) / 3) * (1 - Math.cos(i * radians / total))) })
            .attr("y2", function (d, i) { return ((radius * (l + 1) / 3) * (1 - Math.cos((i + 1) * radians / total))) })
            .style("stroke", "gray")
            .style("stroke-opacity", 0.05)
            .attr("transform", "translate(" + (h / 2 - (radius * (l + 1) / 3)) + "," + (h / 2 - (radius * (l + 1) / 3)) + ")")
    }

    svg2.selectAll("polygon")
        .data([polygonValues])
        .enter()
        .append("polygon")
        .style("stroke", "black")
        .style('fill', "gray")
        .attr("points", function (d) {
            return d.map(function (d) {
                return [d.x, d.y].join(",");
            }).join(" ")
        })
        .attr("transform", function (d) { return ("translate(" + (h / 2 + padding * spider_ratio - (radius * d)) + "," + (h / 2 + padding - (radius * d)) + ")") })
        .attr("opacity", 0.5)

    svg2.selectAll("circle")
        .data(sample_data)
        .enter()
        .append("circle")
        .attr("cx", function (d, i) {

            return ((radius * d) * (1 - Math.sin(i * radians / total)))
        })
        .attr("cy", function (d, i) { return ((radius * d) * (1 - Math.cos(i * radians / total))) })
        .attr("r", 5)
        .attr("transform", function (d) {
            return ("translate(" + (h / 2 + padding * spider_ratio - (radius * d)) + "," + (h / 2 + padding - (radius * d)) + ")")
        })
        .attr("fill", "black")
        .attr("opacity", "0.8")

    spider_text = svg2.selectAll(".label-text")
        .data(['POPULATION', 'INCOME', 'NO2', 'PM25', 'CO', 'Commute', 'MaxAQI'])
        .enter()
        .append("text")
        .attr("class", "label-text")
        .attr("x", function (d, i) { return (radius * (1 - Math.sin(i * radians / total))) })
        .attr("y", function (d, i) { return (radius * (1 - Math.cos(i * radians / total)) + 20) })
        .attr("transform", "translate(" + (radius + (padding * spider_ratio + 1.6) - (radius)) + "," + (radius + padding - (radius)) + ")")
        .text(function (d) { return d })
        .style("font-family", "'Courier New', Courier, monospace")
        .style("font-weight", function (d) { return d == 'PM25' ? "bold" : "" })
        .attr("font-size", function (d) { return d == 'PM25' ? "1.2em" : "1em" })
        .style("fill", function (d, i) {
            return d == 'PM25' ? "orange" : "black"
        })

    spider_text.on("click", function (d) {

        spider_text.transition().duration(125).style("fill", function (l, i) { return l == d ? "orange" : "black" })
            .style("font-weight", function (l) { return l == d ? "bold" : "" })
            .attr("font-size", function (l) { return l == d ? "1.5em" : "1em" })

        if (d == "NO2") {
            var xScale_update = makeXscale(json, 'NO2')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
                .attr("cx", function (d) { return xScale_update(d.properties.NO2) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("NO2")

            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.NO2) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("NO2")

            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))
        } else if (d == "CO") {
            var xScale_update = makeXscale(json, 'CO')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
                .attr("cx", function (d) { return xScale_update(d.properties.CO) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("Carbon Monoxide")

            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.CO) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("Carbon Monoxide")

            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

        } else if (d == "MaxAQI") {
            var xScale_update = makeXscale(json, 'MaxAQI')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
                .attr("cx", function (d) { return xScale_update(d.properties.MaxAQI) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("Air Quality Idex")

            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.MaxAQI) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("Air Quality Index")

            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

        } else if (d == "Commute") {
            var xScale_update = makeXscale(json, 'TRANSIT')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
                .attr("cx", function (d) { return xScale_update(d.properties.TRANSIT) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("Commute (in thousands)")


            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update)
                    .ticks(5)
                    .tickFormat(d => d / 1000))
            // .selectAll("text")
            // .style("text-anchor", "end")
            // .attr("dx", "-.8em")
            // .attr("dy", ".15em")
            // .attr("transform", "rotate(-65)");

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.TRANSIT) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("Commute (in thousands)")


            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update)
                    .ticks(5)
                    .tickFormat(d => d / 1000)
                )

            // .selectAll("text")
            // .style("text-anchor", "end")
            // .attr("dx", "-.8em")
            // .attr("dy", ".15em")
            // .attr("transform", "rotate(-65)");

        } else if (d == "PM25") {
            var xScale_update = makeXscale(json, 'PM25')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
                .attr("cx", function (d) { return xScale_update(d.properties.PM25) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("PM25")

            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.PM25) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("PM25")

            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

        }
        else if (d == "INCOME") {
            var xScale_update = makeXscale(json, 'INCOME')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
                .attr("cx", function (d) { return xScale_update(d.properties.INCOME) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("INCOME")

            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.INCOME) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("INCOME")

            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))
        }
        else if (d == "POPULATION") {
            var xScale_update = makeXscale(json, 'POPULATION')
            svg3.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return INCOME_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.POPULATION) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg3.selectAll(".x-axis-title")
                .text("POP")

            svg3.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))

            svg4.selectAll("circle")
                .data(json.features)
                .transition()
                .duration(600)
                .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
                .attr("cx", function (d) { return xScale_update(d.properties.POPULATION) })
                .attr("r", 5)
                .attr("fill", "gray")

            svg4.selectAll(".x-axis-title")
                .text("INCOME")

            svg4.selectAll(".x-axis")
                .transition()
                .duration(600)
                .call(d3.axisBottom().scale(xScale_update))
        }
    })


    // **********************************
    // ******** THIRD PLOT **************
    // **********************************

    var xScale = makeXscale(json, 'PM25')

    svg3.append("g")
        .attr("class", "x-axis")
        .style("font-family", "'Courier New', Courier, monospace")
        .style("font-size", "0.75em")
        .attr("transform", "translate(" + (0) + "," + (h - padding) + ")")
        .call(d3.axisBottom().scale(xScale))

    inc_circles = svg3.selectAll("circle")
        .data(json.features)
        .enter()
        .append("circle")
        .attr("cy", function (d) { return INCOME_yScale(d.properties.INCOME) })
        .attr("cx", function (d) { return xScale(d.properties.PM25) })
        .attr("r", 5)
        .attr('stroke', 'black')
        .attr("fill", "gray")

    inc_circles.on("mouseover", function (d) {
        d3.select(this)
            .transition()
            .duration(200)
            .style("fill", "orange")
            .attr("r", 10)

        svg.selectAll("path")
            .transition()
            .duration(200)
            .filter(function (l) { return d.properties.KEY == l.properties.KEY })
            .style("fill", "orange")

        var toshow = "NO2: " + Math.ceil(formatAsThousands(d.properties.NO2))
            + " PM25: " + Math.ceil(d.properties.PM25)
            + " MaxAQI: " + Math.ceil(formatAsThousands(d.properties.MaxAQI))
            + " CO2: " + d.properties.CO
            + " Transit: " + d.properties.TRANSIT

        svg.select('text.pollData')
            .text(toshow)
            .attr("position", "absolute")


        var stateD = "County: " + d.properties.CTYNAME.toUpperCase()
            + " Population: " + Math.ceil((d.properties.POPULATION))
            + " Income: " + Math.ceil((d.properties.INCOME))

        svg.select('text.stateData')
            .text(stateD)
            .attr("position", "absolute")

        var update_data = calc_data(d.properties)
        update_poly = []
        update_data.forEach(function (d, i) {
            if (d > 0) {
                x = (radius * d) * (1 - Math.sin(i * (2 * Math.PI) / total))
                x += (h / 2 + padding * spider_ratio - (radius * d))
                y = (radius * d) * (1 - Math.cos(i * (2 * Math.PI) / total))
                y += (h / 2 + padding - (radius * d))
                update_poly.push({ 'x': x, 'y': y })
            }
        })
        svg2.selectAll("circle")
            .data(update_data)
            .transition()
            .duration(200)
            .attr("cx", function (l, i) { return ((radius * l) * (1 - Math.sin(i * radians / total))) })
            .attr("cy", function (l, i) { return ((radius * l) * (1 - Math.cos(i * radians / total))) })
            .attr("r", 5)
            .attr("transform", function (l) { return ("translate(" + (h / 2 + padding * spider_ratio - (radius * l)) + "," + (h / 2 + padding - (radius * l)) + ")") })
            .attr("fill", "black")
            .attr("opacity", "0.8")

        svg2.selectAll("polygon")
            .data([update_poly])
            .transition()
            .duration(200)
            .style("stroke", "black")
            .style('fill', "gray")
            .attr("points", function (l) {
                return l.map(function (l) {
                    return [l.x, l.y].join(",");
                }).join(" ")
            })
            .attr("opacity", 0.5)
    })

    inc_circles.on("mouseout", function (d) {
        svg3.selectAll("circle").style("fill", "gray").attr("r", 5)
        svg4.selectAll("circle").style("fill", "gray").attr("r", 5)
        svg.selectAll("path")
            .transition()
            .duration(300)
            .style("fill", function (l) {
                var value = l.properties.INCOME;
                if (value) {
                    return color(value);
                } else {
                    return "#ccc";
                }
            })
        svg.select('text.pollData').text("")
        svg.select('text.stateData').text("")
    })

    // ***********************************
    // ******** FOURTH PLOT **************
    // ***********************************

    var pop_yScale = d3.scaleLinear()
        .domain([0, d3.max(json.features, function (d) { return d.properties['POPULATION'] })])
        .range([h - padding, 0 + padding])

    var pop_xScale = makeXscale(json, 'PM25')

    svg4.append("g")
        .attr("class", "y-axis")
        .style("font-family", "'Courier New', Courier, monospace")
        .style("font-size", "0.75em")
        .attr("transform", "translate(" + (padding + 40) + "," + 0 + ")")
        .call(d3.axisLeft().scale(pop_yScale))

    svg4.append("g")
        .attr("class", "x-axis")
        .style("font-family", "'Courier New', Courier, monospace")
        .style("font-size", "0.75em")
        .attr("transform", "translate(" + (0) + "," + (h - padding) + ")")
        .call(d3.axisBottom().scale(pop_xScale))

    pop_circles = svg4.selectAll("circle")
        .data(json.features)
        .enter()
        .append("circle")
        .attr("class", "inc-circles")
        .attr("cy", function (d) { return pop_yScale(d.properties.POPULATION) })
        .attr("cx", function (d) { return pop_xScale(d.properties.PM25) })
        .attr("r", 5)
        .attr('stroke', 'black')
        .attr("fill", "gray")

    pop_circles.on("mouseover", function (d) {

        var toshow = "NO2: " + Math.ceil(formatAsThousands(d.properties.NO2))
            + " PM25: " + Math.ceil(d.properties.PM25)
            + " MaxAQI: " + Math.ceil(formatAsThousands(d.properties.MaxAQI))
            + " CO2: " + d.properties.CO
            + " Transit: " + d.properties.TRANSIT

        svg.select('text.pollData')
            .text(toshow)
            .attr("position", "absolute")


        var stateD = "County: " + d.properties.CTYNAME.toUpperCase()
            + " Population: " + Math.ceil((d.properties.POPULATION))
            + " Income: " + Math.ceil((d.properties.INCOME))

        svg.select('text.stateData')
            .text(stateD)
            .attr("position", "absolute")

        d3.select(this)
            .transition()
            .duration(200)
            .style("fill", "orange")
            .attr("r", 10)

        svg.selectAll("path")
            .transition()
            .duration(200)
            .filter(function (l) { return d.properties.KEY == l.properties.KEY })
            .style("fill", "orange")

        var update_data = calc_data(d.properties)
        update_poly = []
        update_data.forEach(function (d, i) {
            x = (radius * d) * (1 - Math.sin(i * (2 * Math.PI) / total))
            x += (h / 2 + padding * spider_ratio - (radius * d))
            y = (radius * d) * (1 - Math.cos(i * (2 * Math.PI) / total))
            y += (h / 2 + padding - (radius * d))
            update_poly.push({ 'x': x, 'y': y })
        })
        svg2.selectAll("circle")
            .data(update_data)
            .transition()
            .duration(200)
            .attr("cx", function (l, i) { return ((radius * l) * (1 - Math.sin(i * radians / total))) })
            .attr("cy", function (l, i) { return ((radius * l) * (1 - Math.cos(i * radians / total))) })
            .attr("r", 5)
            .attr("transform", function (l) { return ("translate(" + (h / 2 + padding * spider_ratio - (radius * l)) + "," + (h / 2 + padding - (radius * l)) + ")") })
            .attr("fill", "black")
            .attr("opacity", "0.8")

        svg2.selectAll("polygon")
            .data([update_poly])
            .transition()
            .duration(200)
            .style("stroke", "black")
            .style('fill', "gray")
            .attr("points", function (l) {
                return l.map(function (l) {
                    return [l.x, l.y].join(",");
                }).join(" ")
            })
            .attr("opacity", 0.5)
    })

    pop_circles.on("mouseout", function (d) {
        svg3.selectAll("circle").style("fill", "gray").attr("r", 5)
        svg4.selectAll("circle").style("fill", "gray").attr("r", 5)
        svg.selectAll("path")
            .transition()
            .duration(300)
            .style("fill", function (l) {
                var value = l.properties.INCOME;
                if (value) {
                    return color(value);
                } else {
                    return "#ccc";
                }
            })
        // svg.select('text.pollData').text("")
        // svg.select('text.stateData').text("")
    })

    // ***********************************
    // ******** SIXTH PLOT **************
    // ***********************************

    function autocomplete(inp, arr) {
        /*the autocomplete function takes two arguments,
        the text field element and an array of possible autocompleted values:*/
        var currentFocus;
        /*execute a function when someone writes in the text field:*/
        var a1 = document.createElement("DIV");
        a1.setAttribute("style", "position: absolute;");
        // appendChild(a1);
        inp.addEventListener("input", function (e) {
            var a, b, i, val = this.value;
            /*close any already open lists of autocompleted values*/
            closeAllLists();
            if (!val) { return false; }
            currentFocus = -1;
            /*create a DIV element that will contain the items (values):*/

            a = document.createElement("DIV");
            a1.appendChild(a)
            a.setAttribute("id", this.id + "autocomplete-list");
            a.setAttribute("class", "autocomplete-items");
            /*append the DIV element as a child of the autocomplete container:*/
            this.parentNode.appendChild(a1);

            /*for each item in the array...*/
            for (i = 0; i < arr.length; i++) {
                /*check if the item starts with the same letters as the text field value:*/
                if (arr[i].substr(0, val.length).toUpperCase() == val.toUpperCase()) {
                    /*create a DIV element for each matching element:*/
                    b = document.createElement("DIV");
                    /*make the matching letters bold:*/
                    b.innerHTML = "<strong>" + arr[i].substr(0, val.length) + "</strong>";
                    b.innerHTML += arr[i].substr(val.length);
                    /*insert a input field that will hold the current array item's value:*/
                    b.innerHTML += "<input type='hidden' value='" + arr[i] + "'>";
                    /*execute a function when someone clicks on the item value (DIV element):*/
                    b.addEventListener("click", function (e) {
                        /*insert the value for the autocomplete text field:*/
                        inp.value = this.getElementsByTagName("input")[0].value;
                        /*close the list of autocompleted values,
                        (or any other open lists of autocompleted values:*/
                        closeAllLists();
                    });
                    a.appendChild(b);
                }
            }
        });
        /*execute a function presses a key on the keyboard:*/
        inp.addEventListener("keydown", function (e) {
            var x = document.getElementById(this.id + "autocomplete-list");
            if (x) x = x.getElementsByTagName("div");
            if (e.keyCode == 40) {
                /*If the arrow DOWN key is pressed,
                increase the currentFocus variable:*/
                currentFocus++;
                /*and and make the current item more visible:*/
                addActive(x);
            } else if (e.keyCode == 38) { //up
                /*If the arrow UP key is pressed,
                decrease the currentFocus variable:*/
                currentFocus--;
                /*and and make the current item more visible:*/
                addActive(x);
            } else if (e.keyCode == 13) {
                /*If the ENTER key is pressed, prevent the form from being submitted,*/
                e.preventDefault();
                if (currentFocus > -1) {
                    /*and simulate a click on the "active" item:*/
                    if (x) x[currentFocus].click();
                }
            }
        });
        function addActive(x) {
            /*a function to classify an item as "active":*/
            if (!x) return false;
            /*start by removing the "active" class on all items:*/
            removeActive(x);
            if (currentFocus >= x.length) currentFocus = 0;
            if (currentFocus < 0) currentFocus = (x.length - 1);
            /*add class "autocomplete-active":*/
            x[currentFocus].classList.add("autocomplete-active");
        }
        function removeActive(x) {
            /*a function to remove the "active" class from all autocomplete items:*/
            for (var i = 0; i < x.length; i++) {
                x[i].classList.remove("autocomplete-active");
            }
        }
        function closeAllLists(elmnt) {
            /*close all autocomplete lists in the document,
            except the one passed as an argument:*/
            var x = document.getElementsByClassName("autocomplete-items");
            for (var i = 0; i < x.length; i++) {
                if (elmnt != x[i] && elmnt != inp) {
                    x[i].parentNode.removeChild(x[i]);
                }
            }
        }
        /*execute a function when someone clicks in the document:*/
        document.addEventListener("click", function (e) {
            closeAllLists(e.target);
        });
    }

    var countries = json.features.map(function (d) { return d.properties.CTYNAME + ", " + d.properties.STNAME })

    autocomplete(document.getElementById("myInput1"), countries);

    var sel1 = d3.select("#myInput1")

    var search_btn1 = d3.select("#search_btn1")
    var search_btn2 = d3.select("#clear_btn")

    var selectedCounties = []

    search_btn1.on("click", function () {
        var selected = sel1.property("value")
        // var selected_country = json.features.filter(function (d) { return d.properties.KEY == selected })
        if (json.features.filter(function (d) {
            sel_arr = selected.split(", ")
            return d.properties.CTYNAME == sel_arr[0] && d.properties.STNAME == sel_arr[1]
        }).length != 0) {
            if (!selectedCounties.includes(selected)) {
                selectedCounties.push(selected)
                // console.log(selectedCounties)
                addTableValues(selectedCounties)
            }
            sel1.property("value", "")
        }
    })

    search_btn2.on("click", function () {
        selectedCounties = []
        d3.select('#table1').selectAll('#dataVals').remove()
        sel1.property("value", "")
    })

    function addTableValues(listCounties) {
        d3.select('#table1').selectAll('#dataVals').remove()
        for (var i = 0; i < listCounties.length; i++) {
            var county1 = json.features.filter(function (d) {

                // return d.properties.CTYNAME == listCounties[i] 
                sel_arr = listCounties[i].split(", ")
                return d.properties.CTYNAME == sel_arr[0] && d.properties.STNAME == sel_arr[1]
            })
            county1 = [county1[0].properties.CTYNAME, county1[0].properties.INCOME, county1[0].properties.POPULATION, county1[0].properties.MaxAQI, county1[0].properties.PM25, county1[0].properties.TRANSIT, county1[0].properties.NO2, county1[0].properties.CO]
            console.log(county1)
            rows = d3.select('table')
                .select('tbody')
                .append('tr')
                .attr('id', 'dataVals')
                .selectAll('td')
                .data(county1)
                .enter()
                .append('td')
                .text(function (d) {
                    if (typeof d == "number") {
                        return Math.floor(d);
                    }
                    else {
                        return d;
                    }
                }
                )
        }
    }
});