function toggleLED(led, state) {
    fetch(`/led?led=${led}&state=${state === 'on' ? 1 : 0}`)  // state 값을 1 또는 0으로 변환
    .then(response => response.text())
    .then(data => alert(data))
    .catch(error => console.error("Error:", error));
}


function setSchedule(led) {
    let onTime = document.getElementById(`onTime${led}`).value;
    let offTime = document.getElementById(`offTime${led}`).value;
    let scheduleEnabled = document.getElementById(`schedule${led}`).checked ? 1 : 0;

    fetch(`/schedule?led=${led}&onTime=${onTime}&offTime=${offTime}&enabled=${scheduleEnabled}`)
    .then(response => response.text())
    .then(data => alert(data))
    .catch(error => console.error("Error:", error));
}

// 스케줄 데이터를 ESP32에서 불러오기
function loadSchedule() {
    fetch("/getSchedule")
    .then(response => response.json())
    .then(data => {
        for (let led = 1; led <= 3; led++) {
            document.getElementById(`onTime${led}`).value = data[led].onTime;
            document.getElementById(`offTime${led}`).value = data[led].offTime;
            document.getElementById(`schedule${led}`).checked = data[led].enabled;
        }
    })
    .catch(error => console.error("Error loading schedule:", error));
}

// 페이지 로드 시 스케줄 정보 가져오기
window.onload = loadSchedule;
