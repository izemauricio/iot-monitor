const secret = require('./auth.secret');
const low = require('lowdb');
const FileSync = require('lowdb/adapters/FileSync')
const adapter = new FileSync('database.json')
const db = low(adapter);
db.defaults({ posts: [], weather: [], count: 0, weatherCount: 0 })
    .write()

const cors = require('cors');
const express = require('express');
const app = express();
app.use(cors());

app.use(function (req, res, next) {
    const { method, url } = req; // originalUrl
    const mem = process.memoryUsage();
    const timeBefore = Date.now();

    const logLabel = `[${method.toUpperCase()}] ${url}`;
    console.log(logLabel);

    for (let key in mem) {
        console.log(`${key} ${Math.round(mem[key] / 1024 / 1024 * 100) / 100} MB`);
    }

    res.on("finish", function () {
        const timeAfter = Date.now();
        console.log(`Took ${timeAfter - timeBefore}ms`);
        console.log('----------------------------------------------------------');
    });

    next()
});

function getTimestamp() {
    /*
    UTC nao tem day light saving, UTC nunce muda.
    O que muda é o timezone dos países que soma ou subtrai from UTC.
    Sete o RTC com UTC
    */
    let date_ob = new Date(); // local time of machine (configurado em utc+3)
    let date = ("0" + date_ob.getDate()).slice(-2);
    let month = ("0" + (date_ob.getMonth() + 1)).slice(-2);
    let year = date_ob.getFullYear();
    let hours = date_ob.getHours();
    let minutes = date_ob.getMinutes();
    let seconds = date_ob.getSeconds();
    let timestamp = "" + year + "-" + month + "-" + date + "-" + hours + "-" + minutes + "-" + seconds;
    let unixtime = Math.round(date_ob.getTime() / 1000); // always return utc time (desconte 3 pq server esta em utc+3)
    return { timestamp, unixtime };
}

function hasApiKeyHasAuthorization(apikey) {
    if (apikey !== secret.apikey) {
        return false;
    } else {
        return true;
    }
}

/*
var date = new Date(unix_timestamp * 1000);
// Hours part from the timestamp
var hours = date.getHours();
// Minutes part from the timestamp
var minutes = "0" + date.getMinutes();
// Seconds part from the timestamp
var seconds = "0" + date.getSeconds();
*/

app.get('/', async (request, response) => {
    console.log('REQUEST MADE AT /');
    return response.status(200).json({ msg: 'oi' });
});

app.get('/saveweather', async (request, response) => {
    console.log('GET /saveweather');

    const apikey = request.query.apikey || '0';
    if (hasApiKeyHasAuthorization(apikey) !== true) {
        return response.status(401).send('Sem autorizacao');
    }

    // server timestamp
    const timestamp = getTimestamp();

    // bme
    const temp = request.query.temp || '0';
    const humi = request.query.humi || '0';
    const pres = request.query.pres || '0';
    const alti = request.query.alti || '0';

    // light
    const lux = request.query.lux || '0';

    // bmp
    const temp2 = request.query.temp2 || '0';
    const pres2 = request.query.pres2 || '0';
    const alti2 = request.query.alti2 || '0';

    // magnetic
    const magx = request.query.magx || '0';
    const magy = request.query.magy || '0';
    const magz = request.query.magz || '0';
    const magazimuth = request.query.azimuth || '0';
    const magbearing = request.query.bearing || '0';
    const magdirection = request.query.direction || 'XXX';

    // mq5 mq7
    const mq5 = request.query.mq5 || '0';
    const mq5ppm = request.query.mq5ppm || '0';
    const mq7 = request.query.mq7 || '0';
    const mq7ppm = request.query.mq7ppm || '0';

    // tiny rtc timer
    nodeunixtime = request.query.unixtime || '0'; // coloquei UTC time no tinyrtc pois javascript Date() espera UTC e nao localtime
    /*
    const rtcTime = {
        year: request.query.ye,
        month: request.query.mo,
        day: request.query.da,
        hour: request.query.hr,
        min: request.query.min,
        second: request.query.sec,
        weekday: request.query.wday,
        unixdays: request.query.unixdays
    }
    */

    // get last
    const lastRow = db.get('weather')
        .takeRight(1)
        .value();

    let id = 1;
    if (lastRow.length > 0) {
        id = lastRow[0].id || 0;
        id = parseInt(id) + 1;
    }

    const newRow = {
        id: id,
        timestamp: timestamp.timestamp, // server time em utc-3
        serverunixtime: timestamp.unixtime, // server unixtime em utc0, new Date(unixtime) espera que unixtime esteja em utc0!
        temp: temp,
        humi: humi,
        pres: pres,
        alti: alti,
        lux: lux,
        temp2: temp2,
        pres2: pres2,
        alti2: alti2,
        mag_x: magx,
        mag_y: magy,
        mag_z: magz,
        mag_azimuth: magazimuth,
        mag_bearing: magbearing,
        mag_direction: magdirection,
        mq5: mq5,
        mq5ppm: mq5ppm,
        mq7: mq7,
        mq7ppm: mq7ppm,
        nodeunixtime: nodeunixtime // nodemcu unixtime em utc-3, deveria vir em utc0
    };

    console.log("NEW ROW WEATHER:");
    console.log(newRow);

    await db.get('weather')
        .push(newRow)
        .write();

    await db.update('weatherCount', n => n + 1)
        .write();

    return response.json({ msg: 'weather saved' });
});

app.get('/savepower', async (request, response) => {
    //"GET /savepower?current=%.2f&a0=%.2f&power=%.2f&voltage=%.2f HTTP/1.0",

    const timestamp = getTimestamp();

    if (!request.query.current) {
        return response.status(400).send('parametros incompletos');
    }
    if (!request.query.a0) {
        return response.status(400).send('parametros incompletos');
    }

    const apikey = request.query.apikey || '0';
    if (hasApiKeyHasAuthorization(apikey) !== true) {
        return response.status(401).send('Sem autorizacao');
    }

    const valueCurrent = request.query.current || '0';
    const valueA0 = request.query.a0 || '0';
    const valuePower = request.query.power || '0';
    const valueVoltage = request.query.voltage || '0';
    const valueEnergy = request.query.energy || '0';
    const valueDuration = request.query.duration || '0';

    // get last
    const lastRow = db.get('posts')
        .takeRight(1)
        .value();

    //console.log('LAST_ROW:');
    //console.log(lastRow);

    let id = 1;
    let lastTotalEnergy = 0;
    if (lastRow.length > 0) {
        id = lastRow[0].id;
        id = parseInt(id) + 1;
        lastTotalEnergy = parseInt(lastRow[0].totalEnergy || '0');
    }

    let newTotalEnergy = lastTotalEnergy + valueEnergy;
    const newRow = {
        timestamp: timestamp.timestamp,
        serverunixtime: timestamp.unixtime,
        id: id,
        current: valueCurrent,
        a0: valueA0,
        valuePower: valuePower,
        valueVoltage: valueVoltage,
        energy: valueEnergy,
        totalEnergy: newTotalEnergy,
        duration: valueDuration,
    };

    console.log('NEW_ROW:');
    console.log(newRow);

    await db.get('posts')
        .push(newRow)
        .write();

    await db.update('count', n => n + 1)
        .write();

    return response.json({ msg: 'saved' });
});

app.get('/getlastpower', async (request, response) => {
    const posts = db.get('posts').takeRight(1).value();
    return response.json(posts);
});

app.get('/getallpower', async (request, response) => {
    const amount = request.query.amount || 1000;
    const posts = db.get('posts').takeRight(amount).value();
    console.log(posts);
    return response.json(posts);
});

app.get('/getallweather', async (request, response) => {
    const amount = request.query.amount || 1000;
    const weather = db.get('weather').takeRight(amount).value();
    console.log(weather);
    return response.json(weather);
});

app.get('/getlastweather', async (request, response) => {
    const posts = db.get('weather').takeRight(1).value();
    return response.json(posts);
});

app.listen(33000, () => {
    console.log("🚀 Server escutando porta 33000...");
});
