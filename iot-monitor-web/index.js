function fetchLastPower() {

    // fetch
    const proxyurl = "";
    fetch(proxyurl + 'http://www.mauriciodz.com:33000/getlastpower')
        .then(response => response.json())
        .then(data => {
            document.getElementById("lastcurrent").innerHTML = data[0].current + "A";
            document.getElementById("lastpowertime").innerHTML = getStringDateFromUtcUnixTime(data[0].serverunixtime);
        });

    //fetch again in 2 seconds
    setTimeout(fetchLastPower, 2500);
}

function fetchWeatherChart() {
    const proxyurl = "";
    fetch(proxyurl + 'http://www.mauriciodz.com:33000/getallweather')
        .then(response => response.json())
        .then(data => {
            document.getElementById("weatherlines").innerHTML = data.length;
            let tableTemp = [];
            let tableHumi = [];
            let tablePres = [];
            let tableLux = [];
            data.forEach(e => {
                tableTemp.push({ label: e.timestamp, y: parseFloat(e.temp) });
                tableHumi.push({ label: e.timestamp, y: parseFloat(e.humi) });
                tableLux.push({ label: e.timestamp, y: parseFloat(e.lux) });
                let pa = parseFloat(e.pres).toFixed(0); // pa
                //let hpa = parseFloat(pa / 100.0).toFixed(2);
                tablePres.push({ label: e.timestamp, y: parseFloat(pa) });
            });


            var chart1 = new CanvasJS.Chart("tempchart", {
                height: 250,
                title: {
                    text: "Temperature Graph [*C]"
                },
                data: [
                    {
                        type: "line",
                        dataPoints: tableTemp,
                        lineColor: "red",
                    }
                ]
            });
            chart1.render();

            var chart2 = new CanvasJS.Chart("humichart", {
                height: 250,
                title: {
                    text: "Hygrometer Monitor [%]"
                },
                data: [
                    {
                        type: "line",
                        dataPoints: tableHumi,
                        lineColor: "blue",
                    }
                ]
            });
            chart2.render();

            var chart3 = new CanvasJS.Chart("preschart", {
                height: 250,
                title: {
                    text: "Barometer Monitor [Pa]"
                },
                data: [
                    {
                        type: "line",
                        dataPoints: tablePres,
                        lineColor: "orange",
                    }
                ]
            });
            chart3.render();

            var chart2 = new CanvasJS.Chart("luxchart", {
                title: {
                    text: "Light Monitor"
                },
                data: [
                    {
                        type: "line",
                        dataPoints: tableLux
                    },
                ]
            });
            chart2.render();
        });
}

function fetchPowerChart() {
    const proxyurl = "";
    fetch(proxyurl + 'http://www.mauriciodz.com:33000/getallpower')
        .then(response => response.json())
        .then(data => {
            document.getElementById("powerlines").innerHTML = data.length;
            let tablePower = [];
            data.forEach(e => {
                tablePower.push({ label: e.timestamp, y: parseFloat(e.current) });
            });

            var chart1 = new CanvasJS.Chart("powergraph", {
                title: {
                    text: "Power Monitor"
                },
                data: [
                    {
                        type: "line",
                        dataPoints: tablePower
                    }
                ]
            });
            chart1.render();
        });
}

function fetchLastWeather() {
    // fetch
    const proxyurl = "";
    fetch(proxyurl + 'http://www.mauriciodz.com:33000/getlastweather')
        .then(response => response.json())
        .then(data => {
            document.getElementById("lasttemp").innerHTML = data[0].temp + "*C";
            document.getElementById("lasthumi").innerHTML = data[0].humi + "%";
            let pa = parseFloat(data[0].pres).toFixed(0); // pa
            let hpa = parseFloat(pa / 100.0).toFixed(2); // hPa
            let localPa = pa.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ","); // thousand separator using regex
            document.getElementById("lastpres").innerHTML = localPa + "Pa";
            document.getElementById("lastalti").innerHTML = data[0].alti + "m";
            document.getElementById("lastlux").innerHTML = parseFloat(data[0].lux).toFixed(0) + "lux";
            document.getElementById("lastazimuth").innerHTML = data[0].mag_azimuth + "deg";
            document.getElementById("lastmag").innerHTML = `X: ${data[0].mag_x}uT<br>Y: ${data[0].mag_y}uT<br>Z: ${data[0].mag_z}uT<br>`;
            document.getElementById("lastweathertime").innerHTML = getStringDateFromUtcUnixTime(data[0].serverunixtime);
        });

    //fetch again in 2 seconds
    setTimeout(fetchLastWeather, 2500);
}

const monthNames = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
];

function getStringDateFromUtcUnixTime(UTCUnixTime) {
    // monta data
    let dateObj = new Date(Number(UTCUnixTime) * 1000);
    let Y = dateObj.getFullYear();
    let M = monthNames[dateObj.getMonth()];
    let D = dateObj.getDate();
    let HH = dateObj.getHours();
    let MM = dateObj.getMinutes();
    let SS = dateObj.getSeconds();
    return `${M}/${D}/${Y}  ${HH}:${MM}:${SS}`;
}

function startup() {
    // fetch last power
    fetchLastPower();
    fetchLastWeather();

    // fetch power chart
    fetchPowerChart();
    fetchWeatherChart();
}