import cv2
import sys
import numpy as np
def draw_image(img,boxes,kpts=None):

    for box in boxes:
        ib = [int(v) for v in box]
        cv2.rectangle(img,tuple(ib[:2]),tuple(ib[2:4]),(0,0,255),2)
    if kpts is None:
        return
    for kpt in kpts:
        ipt = [int(v) for v in kpt]
        for j in range(len(ipt)//2):
            cv2.circle(img,(ipt[2*j],ipt[2*j+1]),2,(0,0,255),-1)


if __name__ == "__main__":
    imgf = sys.argv[1]
    # boxes = boxes=[[388.333,37.2222,435.694,108.056],[185.556,99.7222,235,166.667],[332.153,10.3472,364.583,51.6667],[219.097,0,247.778,22.1528]]

    # kpts=[[403.611,65.3125,425.764,66.3542,415.729,80.625,404.132,90.1389,422.083,91.1111],[203.333,121.806,224.931,119.167,218.611,135.486,208.576,149.167,224.931,147.083],[342.257,22.6563,357.292,23.0556,350.486,31.0764,343.663,38.8889,356.319,39.0972],[226.892,0.243056,240.556,-0.347222,234.115,6.07639,228.507,12.1007,239.549,11.7014]]
    # boxes=[[224.375,490.625,361.25,652.5,0.835938],[884.688,101.25,938.75,171.25,0.789063],[797.188,4.6875,854.688,78.125,0.75],[386.875,401.25,495.625,549.375,0.703125],[1144.69,57.1875,1196.64,118.125,0.648438],[962.969,815.313,1053.13,965.625,0.507813]]
    boxes =[[238,2,408,182],
    [412,2,424,36],
    [14,114,22,124],
    [412,120,424,174]]
    kpts = None
    img = cv2.imread(imgf,cv2.IMREAD_GRAYSCALE)
    img1 = cv2.imread(sys.argv[2],cv2.IMREAD_GRAYSCALE)
    diff = np.abs(img.astype(np.int32)-img1.astype(np.int32)).astype(np.uint8)
    _,diffimg = cv2.threshold(diff,40,255,cv2.THRESH_BINARY)
    diffimg = cv2.cvtColor(diffimg,cv2.COLOR_GRAY2BGR)

    print('imgshape:',img.shape,img.dtype)
    draw_image(diffimg,boxes,kpts)
    cv2.imwrite("xx.jpg",diffimg)
    # cv2.imshow('box',img)
    # cv2.waitKey(0)