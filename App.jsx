import { useEffect, useState } from "react";


import { Bar, Pie, Line } from "react-chartjs-2";
import "chart.js/auto";

const Card = ({ children }) => <div className="bg-white shadow-md rounded-lg p-4">{children}</div>;
const Table = ({ children }) => <table className="min-w-full border">{children}</table>;
const TableHeader = ({ children }) => <thead className="bg-gray-200">{children}</thead>;
const TableRow = ({ children }) => <tr className="border-b">{children}</tr>;
const TableHead = ({ children }) => <th className="p-2 text-left">{children}</th>;
const TableBody = ({ children }) => <tbody>{children}</tbody>;
const TableCell = ({ children }) => <td className="p-2 border">{children}</td>;

const CardContent = ({ children }) => <div>{children}</div>;
export default function Dashboard() {
    const [logs, setLogs] = useState([]);
    const [error, setError] = useState(null);
    const [filter, setFilter] = useState("");

    useEffect(() => {
      const fetchLogs = () => {
          fetch("/traffic_log.json") // Теперь берем логи прямо из public/
              .then((res) => res.json())
              .then((data) => setLogs(data))
              .catch((err) => setError(err.message));
      };
      fetchLogs();
      const interval = setInterval(fetchLogs, 5000);
      return () => clearInterval(interval);
  }, []);

    if (error) return <div className="text-red-500">Error loading logs: {error}</div>;

    const filteredLogs = logs.filter(log => 
        log.source_ip.includes(filter) || 
        log.destination_ip.includes(filter) || 
        log.protocol.toString().includes(filter)
    );

    const protocolCounts = logs.reduce((acc, log) => {
        acc[log.protocol] = (acc[log.protocol] || 0) + 1;
        return acc;
    }, {});

    const timeData = logs.map(log => ({ x: new Date(log.timestamp * 1000), y: 1 }));

    const [isCapturing, setIsCapturing] = useState(false);

    const toggleCapture = async () => {
      const action = isCapturing ? "stop" : "start";
      try {
          await fetch(`/capture-${action}`);
          setIsCapturing(!isCapturing);
      } catch (error) {
          console.error("Failed to toggle capture:", error);
      }
    };

    

    return (
        <div className="p-6 space-y-6">
            <h1 className="text-2xl font-bold">Network Logs Dashboard</h1>
            <input 
                type="text" 
                placeholder="Filter by IP or Protocol" 
                className="p-2 border rounded w-full"
                value={filter} 
                onChange={(e) => setFilter(e.target.value)} 
            />
            <button 
        onClick={toggleCapture} 
        className={`p-2 text-white ${isCapturing ? "bg-red-500" : "bg-green-500"}`}
    >
        {isCapturing ? "Остановить мониторинг" : "Запустить мониторинг"}
    </button>;
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <Card>
                    <CardContent>
                        <h2 className="text-lg font-semibold">Protocol Distribution</h2>
                        <Pie data={{
                            labels: Object.keys(protocolCounts),
                            datasets: [{
                                data: Object.values(protocolCounts),
                                backgroundColor: ["#36A2EB", "#FF6384", "#FFCE56"],
                            }],
                        }} />
                    </CardContent>
                </Card>
                <Card>
                    <CardContent>
                        <h2 className="text-lg font-semibold">Traffic Over Time</h2>
                        <Line data={{
                            datasets: [{
                                label: "Packets over Time",
                                data: timeData,
                                borderColor: "#36A2EB",
                                fill: false,
                            }],
                        }} />
                    </CardContent>
                </Card>
            </div>
            <Card>
                <CardContent>
                    <h2 className="text-lg font-semibold">Logs Table</h2>
                    <Table>
                        <TableHeader>
                            <TableRow>
                                <TableHead>Source IP</TableHead>
                                <TableHead>Destination IP</TableHead>
                                <TableHead>Protocol</TableHead>
                                <TableHead>Timestamp</TableHead>
                            </TableRow>
                        </TableHeader>
                        <TableBody>
                            {filteredLogs.map((log, index) => (
                                <TableRow key={index}>
                                    <TableCell>{log.source_ip}</TableCell>
                                    <TableCell>{log.destination_ip}</TableCell>
                                    <TableCell>{log.protocol}</TableCell>
                                    <TableCell>{new Date(log.timestamp * 1000).toLocaleString()}</TableCell>
                                </TableRow>
                            ))}
                        </TableBody>
                    </Table>
                </CardContent>
            </Card>
        </div>
    );
}
