import paho.mqtt.client as mqtt
import tkinter
import threading
user = "用户名"                            #成功创建thing后返回的username
pwd = "密码"   #成功创建principal后返回的password
endpoint = "ip地址"       #实例(endpoint)地址
port = 1883                                            #endpoint端口
topic = "sw_led"                             #订阅的主题内容

def on_connect(client, userdata, flags, rc):  #连接后返回0为成功
    print("Connected with result code "+str(rc))
    client.subscribe(topic, qos=1) #qos

def on_message(client, userdata, msg):
    print("topic:"+msg.topic+" Message:"+str(msg.payload))

client = mqtt.Client(
    client_id="esp8266", #用来标识设备的ID，用户可自己定义，在同一个实例下，每个实体设备需要有一个唯一的ID
    clean_session=True,
    userdata=None,
)
client.username_pw_set(user, pwd)  # 设置用户名，密码
client.on_connect = on_connect  # 连接后的操作$baidu/iot/shadow/wenshiduesp/update
client.on_message = on_message  # 接受消息的操作

#client.loop_forever()
alive=0
def led():
    global alive
    if alive==0:
        client.publish(topic, "{\"id\":\"on\"}")
        alive=1
        print("led on")
        photo.config(image=ledon)
        button.config(text="关闭led")
    else:
        client.publish(topic, "{\"id\":\"off\"}")
        alive=0
        print("led off")
        photo.config(image=ledoff)
        button.config(text="打开led")
win = tkinter.Tk()
win.geometry("300x120")
led_photo = tkinter.PhotoImage(file="led.png")
ledon = tkinter.PhotoImage(file="ledon.png")
ledoff = tkinter.PhotoImage(file="ledoff.png")
button = tkinter.Button(win,text="打开led",width=15,height=3,command=led)
button.pack(side='right')
photo = tkinter.Label(win,width=200,height=200,image=led_photo)
photo.pack(padx=0,pady=0)
client.connect(endpoint, port, 60)  # 连接服务 keepalive=60
client.publish(topic, "on")
def mqtt_loop():
    client.loop_forever()
def main():
    mqtt=threading.Thread(target=mqtt_loop)
    mqtt.start()
    win.mainloop()
if __name__=='__main__':
    main()
