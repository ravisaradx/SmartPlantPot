const { createProxyMiddleware } = require("http-proxy-middleware");

module.exports = function (app) {
  app.use(
    "/stream",
    createProxyMiddleware({
      target: "http://172.20.10.8", // Replace with your ESP32-CAM IP
      changeOrigin: true,
    })
  );
};