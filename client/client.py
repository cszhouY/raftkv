import socket
import redis

def send_string_to_ip_port(ip, port, string):
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect((ip, port))
    client.sendall(string.encode())
    data = client.recv(1024)
    client.close()
    return data.decode()

def convert_to_resp(data):
    if isinstance(data, str):
        return f"${len(data)}\r\n{data}\r\n"
    elif isinstance(data, list):
        resp = f"*{len(data)}\r\n"
        for item in data:
            resp += convert_to_resp(item)
        return resp
    else:
        raise ValueError("Unsupported data type")

ip = "127.0.0.1"
port = int(input("port: "))
cmd = input("cmd: ")
data = cmd.split()
string = convert_to_resp(data)
response = send_string_to_ip_port(ip, port, string)
print("recv: ", response)