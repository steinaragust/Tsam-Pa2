from multiprocessing import Pool
import requests
if __name__ == '__main__':
    p = Pool(5)
    print p.map(requests.get, ['http://localhost:10000'] * 5)
