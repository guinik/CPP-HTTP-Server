console.log("JS is working! 🚀");

let count = 0;

document.getElementById("counter").textContent = count;

document.getElementById("increment").addEventListener("click", () => {
    count++;
    document.getElementById("counter").textContent = count;
});

document.getElementById("decrement").addEventListener("click", () => {
    count--;
    document.getElementById("counter").textContent = count;
});