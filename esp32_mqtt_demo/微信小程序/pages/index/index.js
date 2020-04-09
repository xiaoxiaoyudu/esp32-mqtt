import mqtt from '../../utils/mqtt.js';

//连接的服务器域名，注意格式！！！
const host = 'wxs://y51pfks.mqtt.iot.bj.baidubce.com/mqtt';

// pages/product/product.js
Page({
  
  /**
   * 页面的初始数据m, m, f
   */
  data: {
   
    scr: "../../images/ledon.png",
    showDialog: false,
    client: null,
    //记录重连的次数
    reconnectCounts: 0,
    ledtxt:"点击打开led",
    now:false,
    //MQTT连接的配置
    options:
     {
      
      clientId: 'espxcx',
      clean: true,
      keepalive: 60,
      password: 'HlN7Y72AD0N8Fvp0',
      username: 'y51pfks/wx_xcx',
      reconnectPeriod: 1000, //1000毫秒，两次重新连接之间的间隔
      connectTimeout: 30 * 1000, //1000毫秒，两次重新连接之间的间隔
      resubscribe: true //如果连接断开并重新连接，则会再次自动订阅已订阅的主题（默认true）
    },
    topic: 
    {
      LEDcontrolTopic: 'sw_led',
    },
    value: {
     // Humdlogo: './../images/humd.png',
      HumdValue: 0,
     // Templogo: './../images/temp.png',
      TempValue: 0,
      winshow:true

    },
    
  

  },
  toggleDialog() {
   
    
    this.setData({
      showDialog: !this.data.showDialog,
    })

  },
  check() {

   if (this.data.now) {
     this.data.client.publish('sw_led', 'n');
     this.setData({
       'scr': "../../images/ledon.png",
       now: false,
     })
  }
   else {
     this.data.client.publish('sw_led', 'f');
     this.setData({
       scr: "../../images/ledoff.png",
       now: true,
     })


   }
   
  },
  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
   



  },

  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {

    


  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {
    var that = this;
    //开始连接
    this.data.client = mqtt.connect(host, this.data.options);
    this.data.client.on('connect', function (connack) {


    })


    //服务器下发消息的回调
    that.data.client.on("message", function (topic, payload) {
      console.log(" 收到 topic:" + topic + " , payload :" + payload)

    })


    //服务器连接异常的回调
    that.data.client.on("error", function (error) {
      console.log(" 服务器 error 的回调" + error)

    })

    //服务器重连连接异常的回调
    that.data.client.on("reconnect", function () {
      console.log(" 服务器 reconnect的回调")

    })


    //服务器连接异常的回调
    that.data.client.on("offline", function (errr) {
      console.log(" 服务器offline的回调")

    })
    this.data.client.subscribe('led', function (err, granted) {
      if (!err) {
        wx.showToast({
          title: '刷新成功'

        })
      } else {
        wx.showToast({
          title: '订阅主题失败',
          icon: 'fail',
          duration: 2000
        })
      }
    })


    this.data.client.publish('led', 'connet');
      //仅订阅单个主题
     



  },

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {

  },

  /**
   * 生命周期函数--监听页面卸载
   */
  onUnload: function () {

  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {

  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {

  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})