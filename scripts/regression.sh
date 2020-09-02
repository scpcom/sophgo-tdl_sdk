counter=0
#!/bin/bash
run() {
    result="PASSED"
    ./"$@" || result="FAILED"
    counter=$((counter+1))
    echo "TEST $counter $1: $result"
}
# For normal CI check
run sample_init
run sample_read_fd /mnt/data/cvimodel/retina_face.cvimodel /mnt/data/image/ryan.png
run sample_read_fr /mnt/data/cvimodel/retina_face.cvimodel /mnt/data/cvimodel/bmface.cvimodel /mnt/data/image/ryan.png
run sample_read_dt /mnt/data/cvimodel/retina_face.cvimodel /mnt/data/image/ryan.png

# For daily build tests
if [[ "$1" != "daily" ]]; then
  exit 0
fi

run reg_wider_face /mnt/data/cvimodel/retina_face.cvimodel /mnt/data/dataset/wider_face /mnt/data/result/wider_face_result
run reg_yolov3 /mnt/data/cvimodel/yolo_v3_416.cvimodel /mnt/data/dataset/coco /mnt/data/result/yolo_result.json
run reg_mobiledetv2 /mnt/data/cvimodel/mobiledetv2_d0.cvimodel /mnt/data/dataset/coco /mnt/data/result/mobiledetv2_result.json
run reg_thermal /mnt/data/thermalfd-v1_resnet18-bifpn-sh.cvimodel /mnt/data/dataset/coco /mnt/data/result/thermal_result.json