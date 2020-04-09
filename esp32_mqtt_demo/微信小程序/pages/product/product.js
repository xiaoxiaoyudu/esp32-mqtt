// pages/product/product.js
Page({

  /**
   * 页面的初始数据
   */
  data: {
    prodata: [
      {
        name: "火灾报警",
        now: true,
        out: "",
        own: true,
        scr: "../../images/fire.png",
        valuv: 1
      },
      {
        name: "灯",
        now: true,
        out: "",
        own: true,
        scr: "../../images/ledon.png",
        valuv: 1
      },
      {
        name: "智能药盒",
        now: true,
        out: "",
        own: true,
        scr: "../../images/eat.png",
        valuv: 1
      },
      {
        name: "智能插座",
        now: false,
        out: "",
        own: true,
        scr: "../../images/ele.png",
        valuv: 1
      },





    ],

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