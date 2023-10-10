const container = require('rhea');
const crypto = require('crypto');
const fs = require("fs");

// 请根据实际情况修改下面的参数
// host，在物联网平台首页，查看开发配置中查看
//var YourHost="iot-06z00xxxxt6xc.amqp.iothub.aliyuncs.com"
var YourHost="iot-06z00g5bmz0mlin.amqp.iothub.aliyuncs.com"
// 客户端ID，可自定义，长度不可超过64个字符
var YourClientId="ic15IdwuJIW.ESP32-Camera_01"
// 账号的 AccessKey。将鼠标移至账号头像上，然后单击AccessKey管理，获取AccessKey ID和AccessKey Secret。
// var YourAccessKeyId="LTAI5tXXXXXXXXXXxLEMGYL2"
// var YourAccessKeySecret="6vi2Txxxw9xxxrwig"
var YourAccessKeyId="LTAI5tExvUAsQXrFCu6QqnQU"
var YourAccessKeySecret="m5AGtvbgg1eXKHnC29cUFvR0WDUEGl"
// 在对应实例的消息转发 > 服务端订阅 > 消费组列表查看您的消费组ID。
// var YourConsumerGroupId="DEFAULT_GROUP"
var YourConsumerGroupId="3MAcgtZCFCQa6pzfpNyq000100"

// 物联网平台首页实例 ID
// var YourIotInstanceId="iot-0600uxtxxsx"
var YourIotInstanceId="iot-06z00g5bmz0mlin"

// 存放完整的图片字符串
var imgStr = ""

// 16进制图片转base64
function to_base64(str) {
    var digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    var base64_rep = "";
    var cnt = 0;
    var bit_arr = 0;
    var bit_num = 0;

    for (var n = 0; n < str.length; ++n) {
        if (str[n] >= 'A' && str[n] <= 'Z') {
            ascv = str.charCodeAt(n) - 55;
        }
        else if (str[n] >= 'a' && str[n] <= 'z') {
            ascv = str.charCodeAt(n) - 87;
        }
        else {
            ascv = str.charCodeAt(n) - 48;
        }
        bit_arr = (bit_arr << 4) | ascv;
        bit_num += 4;
        if (bit_num >= 6) {
            bit_num -= 6;
            base64_rep += digits[bit_arr >>> bit_num];
            bit_arr &= ~(-1 << bit_num);
        }
    }
    if (bit_num > 0) {
        bit_arr <<= 6 - bit_num;
        base64_rep += digits[bit_arr];
    }
    var padding = base64_rep.length % 4;
    if (padding > 0) {
        for (var n = 0; n < 4 - padding; ++n) {
            base64_rep += "=";
        }
    }
    return base64_rep;
}

//创建Connection。
var connection = container.connect({
    //接入域名，请参见AMQP客户端接入说明文档。
    'host': YourHost,
    'port': 5671,
    'transport':'tls',
    'reconnect':true,
    'idle_time_out':60000,
    //userName组装方法，请参见AMQP客户端接入说明文档。
    'username':YourClientId+'|authMode=aksign,signMethod=hmacsha1,timestamp=1573489088171,authId='+YourAccessKeyId+',iotInstanceId='+YourIotInstanceId+',consumerGroupId='+YourConsumerGroupId+'|', 
    //计算签名，password组装方法，请参见AMQP客户端接入说明文档。
    'password': hmacSha1(YourAccessKeySecret, 'authId='+YourAccessKeyId+'&timestamp=1573489088171'),
});

//创建Receiver Link
var receiver = connection.open_receiver();

//接收云端推送消息的回调函数。
container.on('message', function (context) {
    var msg = context.message;
    var messageId = msg.message_id;
    var topic = msg.application_properties.topic;
    var content = Buffer.from(msg.body.content).toString();

    // 输出内容。
    console.log(content);

    // 将接收到的mqtt消息中内容转为json
    var imgBody = JSON.parse(content).items.img.value
    console.log('-------')
    // 如果图片没有传输完毕，则拼接图片
    if (imgBody != 'END') {
        imgStr += imgBody
    } else {
        // 如果图片传输完毕，则将图片转为base64
        console.log('imgStr:')
        console.log(to_base64(imgStr))
        // 配置图片保存路径
        var path = './img/' + new Date().getTime() + '.jpg';
        var dataBuffer = new Buffer(to_base64(imgStr), 'base64'); //把base64码转成buffer对象，
        //用fs将图片写入本地文件
        fs.writeFile(path, dataBuffer, function (err) {
            if (err) {
                console.log(err);
            } else {
                console.log('写入成功！');
            }
        });
        // 图片转换完毕后，清空imgStr，准备接受下一张图片
        imgStr = ""

    }
    //发送ACK，注意不要在回调函数有耗时逻辑。
    context.delivery.accept();
});


//计算password签名。
function hmacSha1(key, context) {
    return Buffer.from(crypto.createHmac('sha1', key).update(context).digest())
        .toString('base64');
}